import paho.mqtt.client as mqtt
import requests
import json
import time

print("Inicializando Motor NLP en la Nube (Gemini 2.5 Flash - Nivel 1)...")

# --- CONFIGURACIÓN DE LA API ---
API_KEY = "CLAVE_API_KEY"

# Apuntamos a la última versión disponible (2.5)
url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={API_KEY}"

print("¡Enlace HTTP REST configurado! Listos para procesar.")

broker = "127.0.0.1"
topic_rx = "aula/nube/analizar"
topic_tx = "aula/nube/resultado"

def on_message(client, userdata, msg):
    texto_recibido = msg.payload.decode("utf-8")
    
    if texto_recibido == "reset_command_internal" or not texto_recibido.strip():
        return

    print(f"\nAnalizando contexto de: '{texto_recibido}'")
    
    instruccion = f"""
    Eres un analizador de emociones estricto. Lee el texto y determina el sentimiento. 
    Responde ÚNICAMENTE con una de estas cinco opciones exactas (incluyendo el emoji), sin texto extra, ni comillas:
    Feliz 😄
    Enojado 😡
    Triste 😢
    Sorprendido 😲
    Neutro 😐
    Texto a evaluar: "{texto_recibido}"
    """
    
    try:
        payload = {
            "contents": [{"parts": [{"text": instruccion}]}],
            "generationConfig": {"temperature": 0.0}
        }
        headers = {'Content-Type': 'application/json'}
        
        response = requests.post(url, headers=headers, json=payload, timeout=10)
        
        if response.status_code == 200:
            data = response.json()
            emocion_detectada = data['candidates'][0]['content']['parts'][0]['text'].strip()
            emocion_limpia = emocion_detectada.replace(".", "").replace('"', '').replace("'", "")
            
            permitidas = ['Feliz 😄', 'Enojado 😡', 'Triste 😢', 'Sorprendido 😲', 'Neutro 😐']
            if emocion_limpia not in permitidas:
                emocion_limpia = "Neutro 😐"
                
            print(f"Resultado Gemini 2.5 -> {emocion_limpia}")
            client.publish(topic_tx, emocion_limpia)
            
        elif response.status_code == 404:
            print(f"Error 404: El modelo gemini-2.5-flash tampoco fue encontrado.")
            print("Consultando a Google los modelos EXACTOS permitidos para tu cuenta...")
            
            # Auto-Descubridor de Modelos
            check_url = f"https://generativelanguage.googleapis.com/v1beta/models?key={API_KEY}"
            res = requests.get(check_url)
            if res.status_code == 200:
                print("\n--- MODELOS COMPATIBLES ENCONTRADOS ---")
                for m in res.json().get('models', []):
                    if 'generateContent' in m.get('supportedGenerationMethods', []):
                        print(f"Copia este nombre: {m['name']}")
                print("---------------------------------------\n")
            client.publish(topic_tx, "Neutro 😐")
            
        else:
            print(f"Error HTTP de Google: {response.status_code} - {response.text}")
            client.publish(topic_tx, "Neutro 😐")
            
    except Exception as e:
        print(f"Error de red/conexión: {e}")
        client.publish(topic_tx, "Neutro 😐")

try:
    from paho.mqtt.enums import CallbackAPIVersion
    client = mqtt.Client(CallbackAPIVersion.VERSION1)
except ImportError:
    client = mqtt.Client()

client.on_message = on_message

while True:
    try:
        client.connect(broker, 1883, 60)
        client.subscribe(topic_rx)
        break
    except Exception as e:
        time.sleep(3)

print("Escuchando mensajes de los estudiantes...")
client.loop_forever()
