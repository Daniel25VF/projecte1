import ssl
import threading
import time
import json
from datetime import datetime
from paho.mqtt import client as mqtt
import mysql.connector

# ---- CONFIG ----
AWS_ENDPOINT = "a1f4m4en5hebao-ats.iot.us-east-1.amazonaws.com"
PORT = 8883
CLIENT_ID = "bbdd"
TOPIC = "rfid/bbdd"   # Publicación de la BBDD
TOPIC2 = "rfid/tags"  # Suscripción al ESP32

PATH_CERT = "certs/adce5360324f06d3821495dcb3a1ed46179432bc57bf7874cae2aec7950376d0-certificate.pem.crt"
PATH_KEY  = "certs/adce5360324f06d3821495dcb3a1ed46179432bc57bf7874cae2aec7950376d0-private.pem.key"
PATH_CA   = "certs/AmazonRootCA1.pem"

# ---- FUNCIONES BASE DE DATOS ----
def conectar_db():
    return mysql.connector.connect(
        host="localhost",
        port=3306,
        user="daniel",
        password="7972",
        database="IticBCN"
    )

def verificar_tarjeta(codi_tarjeta):
    conexion = conectar_db()
    cursor = conexion.cursor()
    # Busca el dispositivo por el código de tarjeta y que esté activo
    query = "SELECT ID_Usuari, Actiu FROM Dispositiu WHERE Codi_Tarjeta = %s"
    cursor.execute(query, (codi_tarjeta,))
    resultado = cursor.fetchone()
    cursor.close()
    conexion.close()
    return resultado  # devuelve (ID_Usuari, Actiu) o None


def registrar_asistencia(id_usuari):
    conexion = conectar_db()
    cursor = conexion.cursor()
    try:
        # Paso 1: Obtener nombre de grupo (ej: "DAM1") de Alumne
        cursor.execute("SELECT Grup FROM Alumne WHERE ID_Usuari = %s", (id_usuari,))
        row = cursor.fetchone()
        if not row:
            print(f"No existe Alumne ligado al usuari {id_usuari}")
            return
        nom_grup = row[0]

        # Paso 2: Traducir el nombre del grupo al ID numérico en la tabla Grup
        cursor.execute("SELECT ID_Grup FROM Grup WHERE Nom = %s", (nom_grup,))
        row = cursor.fetchone()
        if not row:
            print(f"No existe el grupo {nom_grup} en la tabla Grup")
            return
        id_grup = row[0]

        # Paso 3: Buscar la última clase relacionada con este grupo
        cursor.execute("""
            SELECT ID_Classe
            FROM Classe
            WHERE ID_Grup = %s
            ORDER BY ID_Classe DESC
            LIMIT 1
        """, (id_grup,))
        row = cursor.fetchone()
        if not row:
            print(f"No hay clases para el grupo {nom_grup} (ID {id_grup})")
            return
        id_classe = row[0]

        # 4. Registrar asistencia con fecha y hora
        estado = "Present"
        observaciones = "Registre automàtic"
        now = datetime.now()
        fecha_actual = now.strftime('%Y-%m-%d')
        hora_actual = now.strftime('%H:%M:%S')
        insert_query = """
            INSERT INTO Assistencia (ID_Classe, ID_Alumne, Data, Hora, Estat, Observacions)
            VALUES (%s, %s, %s, %s, %s, %s)
        """
        cursor.execute(insert_query, (id_classe, id_usuari, fecha_actual, hora_actual, estado, observaciones))
        conexion.commit()
        print(f"Asistencia registrada para Alumne {id_usuari} en Clase {id_classe} a las {hora_actual}")
        return True
    except Exception as e:
        print("Error al registrar asistencia:", e)
        return False
    finally:
        cursor.close()
        conexion.close()


# ---- CALLBACKS MQTT ----
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("Conectado a AWS IoT con éxito!")
        client.subscribe(TOPIC2, qos=1)
        print(f"Suscrito al topic: {TOPIC2}")
    else:
        print("Error de conexión, código:", rc)

def on_message(client, userdata, msg):
    print(f"[Mensaje crudo recibido en {msg.topic}] {msg.payload}")
    try:
        data = json.loads(msg.payload.decode())
        print("[Mensaje JSON recibido]", data)
        tarjeta = data.get('tag_rfid')
        if tarjeta is None:
            print("No se encontró el campo 'tag_rfid' en el JSON.")
            client.publish(TOPIC, "FORMATO_INVALIDO", qos=1)
            return
    except Exception as e:
        print("Error de formato JSON:", e)
        client.publish(TOPIC, "FORMATO_INVALIDO", qos=1)
        return

    resultado = verificar_tarjeta(tarjeta)

    if resultado:
        id_usuari, actiu = resultado
        if actiu:
            print(f"Acceso permitido para ID_Usuari: {id_usuari}")
            ok = registrar_asistencia(id_usuari)

            if ok:
                mensaje = {
                    "status": "ACCESO_PERMITIDO",
                    "id_usuari": id_usuari
                }
            else:
                mensaje = {
                    "status": "ERROR_ASISTENCIA",
                    "id_usuari": id_usuari
                }

        else:
            print(f"Acceso denegado para ID_Usuari: {id_usuari}")
            mensaje = {
                "status": "ACCESO_DENEGADO",
                "id_usuari": id_usuari
            }

    else:
        print("Tarjeta no reconocida")
        mensaje = {
            "status": "TARJETA_NO_RECONOCIDA"
        }

    client.publish(TOPIC, json.dumps(mensaje), qos=1)

def on_disconnect(client, userdata, rc, properties=None):
    print("Desconectado, rc =", rc)
    if rc != 0:
        def reconnect():
            delay = 1
            while True:
                try:
                    client.reconnect()
                    print("Reconectado")
                    break
                except Exception as e:
                    print(f"Reconexión fallida: {e}, retry en {delay}s")
                    time.sleep(delay)
                    delay = min(delay * 2, 60)
        threading.Thread(target=reconnect).start()

# ---- MQTT CLIENT ----
client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv5)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

client.tls_set(
    ca_certs=PATH_CA,
    certfile=PATH_CERT,
    keyfile=PATH_KEY,
    cert_reqs=ssl.CERT_REQUIRED,
    tls_version=ssl.PROTOCOL_TLSv1_2
)
client.tls_insecure_set(False)

print("Conectando a AWS IoT...")
try:
    client.connect(AWS_ENDPOINT, PORT)
except Exception as e:
    print("Error de conexión:", e)
    exit(1)

client.loop_start()

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Cerrando conexión...")
    client.loop_stop()
    client.disconnect()