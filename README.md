# Componente-demostrativo-de-procesamiento-de-datos-para-la-Carrera-de-Tecnolog-as-de-la-Informaci-n
Este repositorio contiene el entorno de ejecución del componente demostrativo basado en Raspberry Pi, Docker, Node-RED, MQTT, ESP32, ESP32-CAM y servicios Python para procesamiento de datos e inteligencia artificial.

Ver las instrucciones de ejecución en [INSTRUCCIONES_EJECUCION.md](INSTRUCCIONES_EJECUCION.md).

## Dependencias de Node-RED
La carpeta `node_modules` no se versiona en GitHub porque se puede regenerar a partir de `data/package.json` y `data/package-lock.json`.

Para recuperar las dependencias, entra en la carpeta `data` y ejecuta:

```bash
npm install
```

Si necesitas dejar el entorno exactamente como fue definido, usa también `package-lock.json`, ya que fija las versiones instaladas.
