import ssl
import threading
import time
import json
from datetime import datetime
from paho.mqtt import client as mqtt
import mysql.connector

# ---------------------- CONFIGURACIÓ -----------------------
AWS_ENDPOINT = "a1f4m4en5hebao-ats.iot.us-east-1.amazonaws.com"
PORT = 8883
CLIENT_ID = "bbdd"
TOPIC = "rfid/bbdd"   # Tema MQTT on publicarem respostes
TOPIC2 = "rfid/tags"  # Tema MQTT on rebem les targetes RFID del ESP32

# Rutes als certificats i claus per TLS
PATH_CERT = "certs/adce5360324f06d3821495dcb3a1ed46179432bc57bf7874cae2aec7950376d0-certificate.pem.crt"
PATH_KEY  = "certs/adce5360324f06d3821495dcb3a1ed46179432bc57bf7874cae2aec7950376d0-private.pem.key"
PATH_CA   = "certs/AmazonRootCA1.pem"

# ---------------------- FUNCIONS BASE DE DADES -----------------------
def conectar_db():
    """
    Connecta amb la base de dades MySQL local.
    """
    return mysql.connector.connect(
        host="localhost",
        port=3306,
        user="daniel",
        password="7972",
        database="IticBCN"
    )

def verificar_tarjeta(codi_tarjeta):
    """
    Verifica si la targeta RFID existeix i està activa.
    Retorna (ID_Usuari, Actiu) o None si no es troba.
    """
    conexion = conectar_db()
    cursor = conexion.cursor()
    query = "SELECT ID_Usuari, Actiu FROM Dispositiu WHERE Codi_Tarjeta = %s"
    cursor.execute(query, (codi_tarjeta,))
    resultado = cursor.fetchone()
    cursor.close()
    conexion.close()
    return resultado

def registrar_asistencia(id_usuari):
    """
    Registra l’assistència d’un alumne a la classe corresponent.
    Retorna True si l’operació és correcta, False en cas d’error.
    """
    conexion = conectar_db()
    cursor = conexion.cursor()
    try:
        # Obtenir el grup de l’alumne (ex: "DAM1")
        cursor.execute("SELECT Grup FROM Alumne WHERE ID_Usuari = %s", (id_usuari,))
        row = cursor.fetchone()
        if not row:
            print(f"No existeix Alumne associat a l’usuari {id_usuari}")
            return
        nom_grup = row[0]

        # Obtenir l'ID numèric del grup a la taula Grup
        cursor.execute("SELECT ID_Grup FROM Grup WHERE Nom = %s", (nom_grup,))
        row = cursor.fetchone()
        if not row:
            print(f"No existeix el grup {nom_grup} a la taula Grup")
            return
        id_grup = row[0]

        # Obtenir la última classe associada al grup
        cursor.execute("""
            SELECT ID_Classe
            FROM Classe
            WHERE ID_Grup = %s
            ORDER BY ID_Classe DESC
            LIMIT 1
        """, (id_grup,))
        row = cursor.fetchone()
        if not row:
            print(f"No hi ha classes per al grup {nom_grup} (ID {id_grup})")
            return
        id_classe = row[0]

        # Registrar assistència amb data i hora
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
        print(f"Assistència registrada per Alumne {id_usuari} a la Classe {id_classe} a les {hora_actual}")
        return True
    except Exception as e:
        print("Error registrant assistència:", e)
        return False
    finally:
        cursor.close()
        conexion.close()


# ---------------------- CALLBACKS MQTT -----------------------
def on_connect(client, userdata, flags, rc, properties=None):
    """
    S'executa quan el client MQTT es connecta a AWS IoT.
    """
    if rc == 0:
        print("Connectat a AWS IoT amb èxit!")
        client.subscribe(TOPIC2, qos=1)
        print(f"Subscrit al topic: {TOPIC2}")
    else:
        print("Error de connexió, codi:", rc)

def on_message(client, userdata, msg):
    """
    S'executa quan es rep un missatge MQTT.
    Processa el JSON rebut i valida la targeta.
    """
    print(f"[Missatge rebut a {msg.topic}] {msg.payload}")
    try:
        data = json.loads(msg.payload.decode())
        print("[JSON rebut]", data)
        tarjeta = data.get('tag_rfid')
        if tarjeta is None:
            print("No s’ha trobat el camp 'tag_rfid' en el JSON.")
            client.publish(TOPIC, "FORMATO_INVALIDO", qos=1)
            return
    except Exception as e:
        print("Error de format JSON:", e)
        client.publish(TOPIC, "FORMATO_INVALIDO", qos=1)
        return

    # Verifica la targeta a la base de dades
    resultado = verificar_tarjeta(tarjeta)

    if resultado:
        id_usuari, actiu = resultado
        if actiu:
            print(f"Accés permès per ID_Usuari: {id_usuari}")
            ok = registrar_asistencia(id_usuari)
            mensaje = {
                "status": "ACCESO_PERMITIDO" if ok else "ERROR_ASISTENCIA",
                "id_usuari": id_usuari
            }
        else:
            print(f"Accés denegat per ID_Usuari: {id_usuari}")
            mensaje = {
                "status": "ACCESO_DENEGADO",
                "id_usuari": id_usuari
            }
    else:
        print("Targeta no reconeguda")
        mensaje = {
            "status": "TARJETA_NO_RECONOCIDA"
        }

    # Publica la resposta a AWS IoT
    client.publish(TOPIC, json.dumps(mensaje), qos=1)

def on_disconnect(client, userdata, rc, properties=None):
    """
    Gestiona la desconnexió i reintenta reconnectar automàticament.
    """
    print("Desconnectat, rc =", rc)
    if rc != 0:
        def reconnect():
            delay = 1
            while True:
                try:
                    client.reconnect()
                    print("Reconnectat")
                    break
                except Exception as e:
                    print(f"Reconnexió fallida: {e}, reintentar en {delay}s")
                    time.sleep(delay)
                    delay = min(delay * 2, 60)
        threading.Thread(target=reconnect).start()


# ---------------------- CONFIGURACIÓ DEL CLIENT MQTT -----------------------
client = mqtt.Client(client_id=CLIENT_ID, protocol=mqtt.MQTTv5)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

# Configuració TLS/SSL amb certificats
client.tls_set(
    ca_certs=PATH_CA,
    certfile=PATH_CERT,
    keyfile=PATH_KEY,
    cert_reqs=ssl.CERT_REQUIRED,
    tls_version=ssl.PROTOCOL_TLSv1_2
)
client.tls_insecure_set(False)

# Connexió inicial
print("Connectant a AWS IoT...")
try:
    client.connect(AWS_ENDPOINT, PORT)
except Exception as e:
    print("Error de connexió:", e)
    exit(1)

client.loop_start()  

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Tancant connexió...")
    client.loop_stop()
    client.disconnect()
