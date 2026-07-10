FROM nodered/node-red:latest-debian

USER root

# Instalar Python, OpenCV y Scikit-Learn nativo de Linux (32-bits safe)
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-opencv \
    python3-sklearn \
    build-essential \
    python3-dev \
    libffi-dev \
    && rm -rf /var/lib/apt/lists/*

# Instalar librerias de IA, MQTT y Nube (Gemini)
RUN pip3 install sentiment-analysis-spanish paho-mqtt flask google-genai --break-system-packages

USER node-red
