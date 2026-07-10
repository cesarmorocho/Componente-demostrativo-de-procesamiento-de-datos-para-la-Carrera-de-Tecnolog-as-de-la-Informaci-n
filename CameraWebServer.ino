#include "esp_camera.h"              // Importa la librería principal para controlar la cámara del ESP32-CAM.
#include <WiFi.h>                    // Importa la librería Wi-Fi para conectar el ESP32-CAM a una red inalámbrica.
#include <WiFiMulti.h>               // Importa la librería WiFiMulti para registrar varias redes y conectarse automáticamente a una disponible.
#include <PubSubClient.h>            // Importa la librería PubSubClient para manejar la comunicación MQTT.

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"            // Incluye el archivo donde se define el modelo de cámara y los pines correspondientes.

WiFiMulti wifiMulti;                 // Crea un objeto WiFiMulti para administrar múltiples redes Wi-Fi.

// Variables dinámicas para el Broker MQTT
const char* mqtt_server_docente = "192.168.1.30";     // Define la IP del broker MQTT cuando se usa la red del docente o feria.
const char* mqtt_server_casa    = "192.168.100.30";   // Define la IP del broker MQTT cuando se usa la red de casa.
const char* mqtt_server_actual;                        // Declara una variable que almacenará la IP del broker MQTT seleccionado dinámicamente.

const char* topic_ip_camara = "aula/vision/ip_camara"; // Define el tópico MQTT donde la ESP32-CAM publicará su dirección IP.

WiFiClient espClient;                // Crea un cliente Wi-Fi base que será usado por el cliente MQTT.
PubSubClient client(espClient);      // Crea el cliente MQTT utilizando la conexión Wi-Fi definida en espClient.

void startCameraServer();            // Declara la función que inicia el servidor web de la cámara.
void setupLedFlash();                // Declara la función que configura el flash LED de la cámara, si el modelo lo permite.

void setup_wifi() {                  // Define la función encargada de conectar la ESP32-CAM a una red Wi-Fi.
  delay(10);                         // Realiza una pequeña pausa inicial para estabilizar el módulo Wi-Fi.
  Serial.println("\nBuscando redes Wi-Fi..."); // Muestra en el monitor serial que se está iniciando la búsqueda de redes.
  WiFi.mode(WIFI_STA);               // Configura el ESP32-CAM en modo estación para conectarse a un router o punto de acceso.
  WiFi.setSleep(false);              // Desactiva el ahorro de energía del Wi-Fi para mantener estable el streaming de video.

  // Red del docente
  wifiMulti.addAP("prueba pi", "");  // Registra la red del docente o feria, en este caso sin contraseña.
  
  // Red de casa
  wifiMulti.addAP("NETLIFE - GABRIELA", "TU_CONTRASENA_WIFI"); // Registra la red de casa; la contraseña real debe mantenerse privada.

  while (wifiMulti.run() != WL_CONNECTED) { // Repite el proceso hasta que la ESP32-CAM se conecte a una red registrada.
    delay(500);                             // Espera medio segundo antes de volver a intentar la conexión.
    Serial.print(".");                      // Imprime un punto en el monitor serial para indicar que sigue intentando conectarse.
  }

  Serial.println("\nWiFi conectado!");      // Muestra que la conexión Wi-Fi fue exitosa.
  Serial.print("Red: ");                    // Imprime una etiqueta para mostrar la red conectada.
  Serial.println(WiFi.SSID());              // Muestra el nombre de la red Wi-Fi a la que se conectó la ESP32-CAM.
  Serial.print("IP Asignada: ");            // Imprime una etiqueta para mostrar la IP asignada.
  Serial.println(WiFi.localIP());           // Muestra la dirección IP local asignada a la ESP32-CAM.

  if (WiFi.SSID() == "prueba pi") {         // Verifica si la ESP32-CAM se conectó a la red del docente o feria.
    mqtt_server_actual = mqtt_server_docente; // Asigna como broker MQTT la IP correspondiente a la red del docente.
  } else {                                  // Si no se conectó a la red del docente, asume que está en la red de casa.
    mqtt_server_actual = mqtt_server_casa;  // Asigna como broker MQTT la IP correspondiente a la red de casa.
  }
}

void reconnect_mqtt() {                     // Define la función encargada de conectar o reconectar la ESP32-CAM al broker MQTT.
  while (!client.connected()) {             // Mientras el cliente MQTT no esté conectado, seguirá intentando conectarse.
    Serial.print("Conectando a MQTT...");   // Muestra en el monitor serial que se está intentando conectar al broker MQTT.
    
    if (client.connect("ESP32_CAM_Client")) { // Intenta conectar al broker MQTT usando el identificador ESP32_CAM_Client.
      Serial.println("¡Conectado!");        // Muestra que la conexión MQTT fue exitosa.
      
      // ANUNCIAR LA IP DE LA CÁMARA AL SERVIDOR PYTHON
      String ipStr = WiFi.localIP().toString(); // Convierte la dirección IP local de la cámara a una cadena de texto.
      client.publish(topic_ip_camara, ipStr.c_str(), true); // Publica la IP en MQTT como mensaje retenido para que otros servicios puedan leerla después.
      Serial.println("IP publicada en MQTT.");  // Muestra en el monitor serial que la IP fue publicada correctamente.
    } else {                                    // Si la conexión MQTT falla, ejecuta este bloque.
      Serial.print("Fallo, rc=");               // Imprime un mensaje indicando que ocurrió un fallo.
      Serial.print(client.state());             // Muestra el código de estado del error MQTT.
      Serial.println(" reintentando en 5s");    // Indica que se volverá a intentar la conexión después de 5 segundos.
      delay(5000);                              // Espera 5 segundos antes de intentar conectarse nuevamente.
    }
  }
}

void setup() {                                  // Define la función de configuración inicial, que se ejecuta una sola vez.
  Serial.begin(115200);                         // Inicia la comunicación serial a 115200 baudios para depuración.
  Serial.setDebugOutput(true);                  // Activa la salida de depuración por el monitor serial.
  Serial.println();                             // Imprime una línea en blanco en el monitor serial.

  camera_config_t config;                       // Crea una estructura de configuración para la cámara.
  config.ledc_channel = LEDC_CHANNEL_0;         // Asigna el canal LEDC utilizado para generar la señal de reloj de la cámara.
  config.ledc_timer = LEDC_TIMER_0;             // Asigna el temporizador LEDC utilizado para la señal de reloj.
  config.pin_d0 = Y2_GPIO_NUM;                  // Asigna el pin de datos D0 de la cámara.
  config.pin_d1 = Y3_GPIO_NUM;                  // Asigna el pin de datos D1 de la cámara.
  config.pin_d2 = Y4_GPIO_NUM;                  // Asigna el pin de datos D2 de la cámara.
  config.pin_d3 = Y5_GPIO_NUM;                  // Asigna el pin de datos D3 de la cámara.
  config.pin_d4 = Y6_GPIO_NUM;                  // Asigna el pin de datos D4 de la cámara.
  config.pin_d5 = Y7_GPIO_NUM;                  // Asigna el pin de datos D5 de la cámara.
  config.pin_d6 = Y8_GPIO_NUM;                  // Asigna el pin de datos D6 de la cámara.
  config.pin_d7 = Y9_GPIO_NUM;                  // Asigna el pin de datos D7 de la cámara.
  config.pin_xclk = XCLK_GPIO_NUM;              // Asigna el pin de reloj externo XCLK de la cámara.
  config.pin_pclk = PCLK_GPIO_NUM;              // Asigna el pin PCLK, usado para sincronizar los datos de imagen.
  config.pin_vsync = VSYNC_GPIO_NUM;            // Asigna el pin VSYNC, usado para sincronización vertical de la imagen.
  config.pin_href = HREF_GPIO_NUM;              // Asigna el pin HREF, usado para indicar líneas válidas de imagen.
  config.pin_sccb_sda = SIOD_GPIO_NUM;          // Asigna el pin SDA del bus SCCB/I2C usado para configurar el sensor de imagen.
  config.pin_sccb_scl = SIOC_GPIO_NUM;          // Asigna el pin SCL del bus SCCB/I2C usado para configurar el sensor de imagen.
  config.pin_pwdn = PWDN_GPIO_NUM;              // Asigna el pin de apagado de la cámara, si el modelo lo utiliza.
  config.pin_reset = RESET_GPIO_NUM;            // Asigna el pin de reinicio de la cámara, si el modelo lo utiliza.
  config.xclk_freq_hz = 5000000;                // Define la frecuencia del reloj externo de la cámara en 5 MHz.
  config.frame_size = FRAMESIZE_VGA;            // Define inicialmente el tamaño de imagen en formato VGA.
  config.pixel_format = PIXFORMAT_JPEG;         // Configura el formato de imagen JPEG, adecuado para streaming.
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;    // Define que la cámara capture un nuevo frame cuando el buffer esté vacío.
  config.fb_location = CAMERA_FB_IN_PSRAM;      // Indica que el frame buffer se almacenará en PSRAM si está disponible.
  config.jpeg_quality = 12;                     // Define la calidad JPEG inicial; menor valor significa mayor calidad.
  config.fb_count = 1;                          // Define la cantidad inicial de buffers de imagen.

  if (config.pixel_format == PIXFORMAT_JPEG) {  // Verifica si el formato configurado es JPEG.
    if (psramFound()) {                         // Comprueba si la placa tiene memoria PSRAM disponible.
      config.jpeg_quality = 10;                 // Mejora la calidad JPEG si existe PSRAM.
      config.fb_count = 2;                      // Usa dos buffers para mejorar la fluidez del streaming.
      config.grab_mode = CAMERA_GRAB_LATEST;    // Captura siempre el frame más reciente disponible.
    } else {                                    // Si no se detecta PSRAM, se usa una configuración más conservadora.
      config.frame_size = FRAMESIZE_SVGA;       // Ajusta el tamaño de imagen a SVGA.
      config.fb_location = CAMERA_FB_IN_DRAM;   // Almacena el frame buffer en DRAM porque no hay PSRAM disponible.
    }
  } else {                                      // Si el formato no es JPEG, se aplica una configuración alternativa.
    config.frame_size = FRAMESIZE_240X240;      // Define un tamaño de imagen pequeño de 240 x 240 píxeles.
    #if CONFIG_IDF_TARGET_ESP32S3               // Compila esta sección solo si el objetivo es un ESP32-S3.
    config.fb_count = 2;                        // Usa dos buffers en ESP32-S3 para mejorar el manejo de imágenes.
    #endif                                      // Finaliza la condición de compilación para ESP32-S3.
  }

#if defined(CAMERA_MODEL_ESP_EYE)               // Compila esta sección solo si el modelo de cámara definido es ESP-EYE.
  pinMode(13, INPUT_PULLUP);                    // Configura el pin 13 como entrada con resistencia pull-up interna.
  pinMode(14, INPUT_PULLUP);                    // Configura el pin 14 como entrada con resistencia pull-up interna.
#endif                                           // Finaliza la condición de compilación para CAMERA_MODEL_ESP_EYE.

  // camera init
  esp_err_t err = esp_camera_init(&config);      // Inicializa la cámara usando la configuración definida.
  
  if (err != ESP_OK) {                           // Verifica si la inicialización de la cámara falló.
    Serial.printf("Camera init failed with error 0x%x", err); // Muestra el código de error si la cámara no inicia correctamente.
    return;                                      // Detiene la ejecución de setup si la cámara no pudo inicializarse.
  }

  sensor_t *s = esp_camera_sensor_get();         // Obtiene un puntero al sensor de cámara para modificar ajustes internos.
  
  if (s->id.PID == OV3660_PID) {                 // Verifica si el sensor detectado es el modelo OV3660.
    s->set_vflip(s, 1);                          // Invierte verticalmente la imagen para corregir la orientación.
    s->set_brightness(s, 1);                     // Aumenta ligeramente el brillo de la imagen.
    s->set_saturation(s, -2);                    // Disminuye la saturación de color de la imagen.
  }
  
  if (config.pixel_format == PIXFORMAT_JPEG) {   // Verifica si el formato de imagen configurado es JPEG.
    s->set_framesize(s, FRAMESIZE_QVGA);         // Reduce el tamaño final de imagen a QVGA para mejorar la transmisión.
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM) // Compila esta sección para modelos M5STACK compatibles.
  s->set_vflip(s, 1);                            // Invierte verticalmente la imagen para estos modelos.
  s->set_hmirror(s, 1);                          // Invierte horizontalmente la imagen para estos modelos.
#endif                                           // Finaliza la condición de compilación para modelos M5STACK.

#if defined(CAMERA_MODEL_ESP32S3_EYE)            // Compila esta sección solo si el modelo es ESP32S3_EYE.
  s->set_vflip(s, 1);                            // Invierte verticalmente la imagen para corregir orientación.
#endif                                           // Finaliza la condición de compilación para ESP32S3_EYE.

#if defined(LED_GPIO_NUM)                        // Compila esta sección solo si existe un pin LED definido.
  setupLedFlash();                               // Configura el flash LED de la cámara.
#endif                                           // Finaliza la condición de compilación para LED_GPIO_NUM.

  // 1. Llamada a la conexión inteligente de Wi-Fi
  setup_wifi();                                  // Ejecuta la conexión Wi-Fi automática mediante WiFiMulti.

  // 2. Iniciar el Servidor de la Cámara
  startCameraServer();                           // Inicia el servidor web que permite acceder al streaming de la cámara.

  // 3. Imprimir el mensaje de éxito original
  Serial.print("Camera Ready! Use 'http://");    // Imprime el inicio de la URL de acceso a la cámara.
  Serial.print(WiFi.localIP());                  // Imprime la IP local asignada a la ESP32-CAM.
  Serial.println("' to connect");               // Completa el mensaje indicando que se puede conectar desde un navegador.

  // 4. Configurar cliente MQTT
  client.setServer(mqtt_server_actual, 1883);    // Configura el broker MQTT seleccionado y el puerto estándar 1883.
}

void loop() {                                    // Define el bucle principal que se ejecuta continuamente.
  if (!client.connected()) {                     // Verifica si el cliente MQTT está desconectado.
    reconnect_mqtt();                            // Si está desconectado, llama a la función de reconexión MQTT.
  }
  
  client.loop();                                 // Mantiene activa la comunicación MQTT y procesa eventos pendientes.
  
  delay(100);                                    // Espera 100 milisegundos antes de repetir el ciclo principal.
}