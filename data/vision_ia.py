# -*- coding: utf-8 -*-
import cv2
import paho.mqtt.client as mqtt
import time
import numpy as np
from flask import Flask, Response
import os
import requests
import base64
import threading
import re

print("Inicializando Motor de Visión Híbrida (EdgeBoxes + CloudGemini 2.5)...")

# --- 1. CARGA DE MODELO LOCAL (Rastreo Rápido) ---
# Usamos Haar Cascade local para recuadros rápidos sin lentitud
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ruta_rostro = os.path.join(BASE_DIR, 'haarcascade_frontalface_default.xml')
face_cascade = cv2.CascadeClassifier(ruta_rostro)

# --- 2. CONFIGURACIÓN DE LA API GEMINI (Análisis Profundo) ---
API_KEY = "AIzaSyAFJveoELBhzlDfDtGfy9aPf-ln6EL6b_Q"
URL_GEMINI = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={API_KEY}"

# --- 3. CONFIGURACIÓN MQTT Y GLOBALES ---
broker = "127.0.0.1" 
url_camara = None
luminosidad_estado = "CALCULANDO..."
ia_resumen_estructurado = ["Esperando analisis Cloud..."] # Lista para saltos de línea
personas_count = 0 # Conteo numérico local
is_analyzing = False 
# Variables para los recuadros locales
last_faces = []

def on_message(client, userdata, msg):
    global url_camara
    if msg.topic == "aula/vision/ip_camara":
        ip = msg.payload.decode('utf-8')
        url_camara = f"http://{ip}:81/stream"
        print(f"\n[SISTEMA] IP de la cámara recibida: {url_camara}")

try:
    from paho.mqtt.enums import CallbackAPIVersion
    client = mqtt.Client(CallbackAPIVersion.VERSION1)
except ImportError:
    client = mqtt.Client()

client.on_message = on_message

while True:
    try:
        client.connect(broker, 1883, 60)
        client.subscribe("aula/vision/ip_camara")
        print("¡Broker conectado! Esperando IP de la cámara...")
        break
    except Exception as e:
        time.sleep(3)

app = Flask(__name__)

# --- 4. FUNCIONES DE INTELIGENCIA ARTIFICIAL HÍBRIDA ---

def medir_luminosidad(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    brillo_promedio = np.mean(gray)
    if brillo_promedio < 40: return "POCA LUZ"
    elif brillo_promedio > 210: return "MUCHA LUZ"
    else: return "ILUMINACION OPTIMA"

def limpiar_tildes(texto):
    """Elimina tildes y ñ para OpenCV"""
    reemplazos = (("á", "a"), ("é", "e"), ("í", "i"), ("ó", "o"), ("ú", "u"), ("ñ", "n"), 
                  ("Á", "A"), ("É", "E"), ("Í", "I"), ("Ó", "O"), ("Ú", "U"), ("Ñ", "N"))
    for a, b in reemplazos:
        texto = texto.replace(a, b)
    return texto

def enviar_a_gemini(frame_copia):
    """Analiza la imagen profundamente con Gemini Vision"""
    global ia_resumen_estructurado, personas_count, is_analyzing
    is_analyzing = True
    
    try:
        # Optimizar imagen para el envío
        small_frame = cv2.resize(frame_copia, (640, 480))
        _, buffer = cv2.imencode('.jpg', small_frame)
        img_b64 = base64.b64encode(buffer).decode('utf-8')
        
        # PROMPT EXTREMADAMENTE ESTRICTO para formato de saltos de línea (lista con ';')
        prompt = """
        Eres una IA de vigilancia avanzada. Observa esta imagen de un salón.
        Responde ÚNICAMENTE con una lista estructurada, sin introducciones. 
        Usa el carácter ';' como separador de saltos de línea para el HUD. Sigue este formato exacto:
        Total: [conteo] personas;
        [X] hombre, [Y] mujer;
        [Combinaciones lentes, ej: '1 mujer con lentes, 1 hombre sin lentes'];
        Objetos: [laptops, celulares, mochilas]
        Si no hay humanos, di '0 personas;Nadie detectado;Nadie detectado;No se detectan objetos'.
        Ejemplo de respuesta válida: 2 personas;1 hombre, 1 mujer;1 hombre sin lentes, 1 mujer con lentes;1 laptop.
        """
        
        payload = {
            "contents": [{
                "parts": [
                    {"text": prompt},
                    {"inline_data": {"mime_type": "image/jpeg", "data": img_b64}}
                ]
            }],
            "generationConfig": {"temperature": 0.0} 
        }
        
        headers = {'Content-Type': 'application/json'}
        response = requests.post(URL_GEMINI, headers=headers, json=payload, timeout=25)
        
        if response.status_code == 200:
            data = response.json()
            respuesta_cruda = data['candidates'][0]['content']['parts'][0]['text'].strip()
            
            # Limpiar tildes y dividir en lista usando el separador ';'
            respuesta_limpia = limpiar_tildes(respuesta_cruda)
            ia_resumen_estructurado = respuesta_limpia.split(';')
            
            # Extraer número de personas (primer elemento de la lista) para el Dashboard
            aforo_match = re.search(r'([0-9]+)\s*(?:personas?|total)', ia_resumen_estructurado[0].lower())
            
            if aforo_match:
                personas_count = int(aforo_match.group(1))
            else:
                if "vacio" in ia_resumen_estructurado[0].lower() or "0" in ia_resumen_estructurado[0].lower():
                    personas_count = 0

            print(f"👁️ Cloud AI: {ia_resumen_estructurado} | Aforo: {personas_count}")
            
            # Enviar datos a Node-RED
            client.publish("aula/vision/personas", str(personas_count)) 
            client.publish("aula/vision/ia_avanzada", respuesta_cruda) # Texto crudo para UTF8 en dashboard
            
        else:
            ia_resumen_estructurado = [f"Error Gemini: {response.status_code}"]
            print(f"Error API: {response.text}")
            
    except Exception as e:
        ia_resumen_estructurado = ["Error de conexion Cloud."]
        print(f"Error Hilo IA: {e}")
    finally:
        is_analyzing = False

def dibujar_hud_avanzado(frame, luminosidad, ia_lista):
    """Dibuja texto pequeño, con saltos de línea y fondo translúcido en la esquina."""
    
    # --- Etiqueta de Luz (TOP) ---
    color_luz = (0, 0, 255) if "Critico" in luminosidad else (0, 255, 255) if "Saturado" in luminosidad else (0, 255, 0)
    cv2.putText(frame, f"Luz: {luminosidad}", (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,0,0), 3)
    cv2.putText(frame, f"Luz: {luminosidad}", (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color_luz, 1)

    # --- Etiqueta de la IA (BOTTOM-LEFT REPOSICIONADA Y REDUCIDA) ---
    y0 = frame.shape[0] - (len(ia_lista) * 20) - 20 # Calculamos inicio hacia arriba
    
    for i, linea in enumerate(ia_lista):
        # Limpieza de seguridad de caracteres no ASCII residuales
        linea_final = linea.strip().replace('\n', '').replace('"', '').replace("'", "")
        if not linea_final: continue

        y = y0 + i * 20
        
        # Prefijo para el título (Solo la primera línea)
        prefix = "Resumen Aula:" if i == 0 else " "
        
        # Texto mucho más pequeño y discreto (fontScale 0.45)
        # Sombra negra
        cv2.putText(frame, f"{prefix} {linea_final}", (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0,0,0), 3)
        # Texto amarillo discreto
        cv2.putText(frame, f"{prefix} {linea_final}", (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (255, 255, 0), 1)

# --- 5. MOTOR PRINCIPAL DE VIDEO ---
def generate_frames():
    global url_camara, luminosidad_estado, is_analyzing, ia_resumen_estructurado, last_faces
    cap = None
    ultimo_analisis = time.time()
    last_time_boxes = time.time()

    while True:
        if url_camara is None:
            time.sleep(1)
            continue
            
        if cap is None or not cap.isOpened():
            print(f"Conectando cámara en {url_camara}...")
            cap = cv2.VideoCapture(url_camara)
            if not cap.isOpened():
                time.sleep(2)
                continue

        success, frame = cap.read()
        
        if not success:
            time.sleep(0.5)
            cap.release()
            cap = None 
            continue

        tiempo_actual = time.time()
        
        # 1. Medir luz en tiempo real (Edge)
        luminosidad_estado = medir_luminosidad(frame)
        
        # 2. Detección Local de Rostros (Recuadros rápidos a 10 FPS)
        if tiempo_actual - last_time_boxes > 0.1: # Ejecutar recuadros cada 0.1 seg (10 FPS)
            small_frame = cv2.resize(frame, (0, 0), fx=0.5, fy=0.5)
            gray = cv2.cvtColor(small_frame, cv2.COLOR_BGR2GRAY)
            faces = face_cascade.detectMultiScale(gray, scaleFactor=1.05, minNeighbors=2, minSize=(10, 10))
            
            # Escalar recuadros al tamaño original
            last_faces = []
            for (x, y, w, h) in faces:
                last_faces.append((x*2, y*2, w*2, h*2))
            
            last_time_boxes = tiempo_actual

        # 3. Dibujar Recuadros Verdes sobre personas en vivo
        for (x, y, w, h) in last_faces:
            cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 1) # Grosor 1 para ser discreto

        # 4. Enviar a Gemini (Cloud AI) cada 8 segundos
        if tiempo_actual - ultimo_analisis > 8.0 and not is_analyzing:
            if "POCA LUZ" not in luminosidad_estado:
                # Disparamos el análisis en un hilo separado
                hilo_ia = threading.Thread(target=enviar_a_gemini, args=(frame.copy(),))
                hilo_ia.daemon = True
                hilo_ia.start()
                ultimo_analisis = tiempo_actual
            else:
                ia_resumen_estructurado = ["Analisis pausado: Luz insuficiente"]

        # 5. Dibujar HUD Avanzado (Luz y Resumen ESTRUCTURADO en la esquina inferior)
        dibujar_hud_avanzado(frame, luminosidad_estado, ia_resumen_estructurado)

        # Empaquetar video para Flask
        ret, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == "__main__":
    client.loop_start()
    app.run(host='0.0.0.0', port=5050, threaded=True)
