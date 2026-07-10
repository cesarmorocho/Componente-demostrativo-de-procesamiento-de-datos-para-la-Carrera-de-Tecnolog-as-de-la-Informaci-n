#include <WiFi.h>                 // Importa la librería principal para utilizar la conectividad Wi-Fi del ESP32.
#include <WiFiMulti.h>            // Importa la librería WiFiMulti para registrar varias redes Wi-Fi y conectarse automáticamente a una disponible.
#include <PubSubClient.h>         // Importa la librería PubSubClient para manejar la comunicación MQTT.
#include <ArduinoJson.h>          // Importa la librería ArduinoJson para crear y enviar datos en formato JSON.

// --- Configuración Wi-Fi y MQTT Dinámica ---
WiFiMulti wifiMulti;              // Crea un objeto WiFiMulti para gestionar múltiples redes Wi-Fi.

const char* mqtt_server_docente = "192.168.1.30";    // Define la dirección IP del broker MQTT cuando se usa la red del docente o feria.
const char* mqtt_server_casa    = "192.168.100.30";  // Define la dirección IP del broker MQTT cuando se usa la red de casa.
const char* mqtt_server_actual;                       // Declara una variable que almacenará dinámicamente la IP del broker MQTT seleccionado.

// Tópicos
const char* mqtt_topic_pub   = "aula/datos";                     // Define el tópico MQTT donde el ESP32 publicará los datos de sensores.
const char* mqtt_topic_sub   = "aula/control";                   // Define el tópico MQTT donde el ESP32 recibirá comandos generales de control.
const char* topic_sc5_buzzer = "escenario5/actuadores/buzzer";   // Define el tópico MQTT específico para activar el buzzer en el Escenario 5.
const char* topic_sc5_ultra  = "escenario5/sensores/ultrasonico";// Define el tópico MQTT específico para publicar la distancia del sensor ultrasónico en el Escenario 5.

// --- Clientes ---
WiFiClient espClient;             // Crea un cliente Wi-Fi base que será usado por el cliente MQTT.
PubSubClient client(espClient);   // Crea el cliente MQTT utilizando la conexión Wi-Fi definida en espClient.

// --- PINES ---
const int SOUND_PIN = 35;         // Define el pin GPIO 35 como entrada analógica para el sensor de sonido.
const int LDR_PIN = 34;           // Define el pin GPIO 34 como entrada analógica para la fotorresistencia LDR.
const int PIR_PIN = 25;           // Define el pin GPIO 25 como entrada digital para el sensor PIR.
const int TRIG_PIN = 33;          // Define el pin GPIO 33 como salida digital TRIG del sensor ultrasónico.
const int ECHO_PIN = 32;          // Define el pin GPIO 32 como entrada digital ECHO del sensor ultrasónico.
const int BUZZER_PIN = 26;        // Define el pin GPIO 26 como salida digital para controlar el buzzer.
const int SEMAFORO_R_PIN = 23;    // Define el pin GPIO 23 como salida digital para el LED rojo del semáforo.
const int SEMAFORO_Y_PIN = 22;    // Define el pin GPIO 22 como salida digital para el LED amarillo del semáforo.
const int SEMAFORO_G_PIN = 21;    // Define el pin GPIO 21 como salida digital para el LED verde del semáforo.
const int FAN_PIN = 19;           // Define el pin GPIO 19 como salida digital para controlar el ventilador o motor DC.

// --- Ajustes Sonido ---
const int NOISE_THRESHOLD = 200;  // Define el umbral mínimo de amplitud para considerar que existe sonido relevante.

JsonDocument doc;                 // Crea un documento JSON donde se almacenarán los datos antes de enviarlos por MQTT.
bool hacerBip = false;            // Declara una bandera lógica para indicar si debe realizarse un bip corto con el buzzer.

void callback(char* topic, byte* message, unsigned int length) {   // Define la función que se ejecuta automáticamente cuando llega un mensaje MQTT.
  String topicStr = String(topic);                                  // Convierte el tópico recibido de tipo char* a un objeto String.
  String messageTemp;                                               // Crea una variable temporal para almacenar el mensaje recibido como texto.

  for (int i = 0; i < length; i++) messageTemp += (char)message[i]; // Recorre byte por byte el mensaje MQTT y lo convierte en una cadena de texto.
  
  if (topicStr == mqtt_topic_sub) {                                 // Verifica si el mensaje llegó por el tópico general de control.
    if (messageTemp == "ON") digitalWrite(BUZZER_PIN, LOW);         // Si el mensaje es ON, activa el buzzer usando lógica inversa.
    else if (messageTemp == "OFF") digitalWrite(BUZZER_PIN, HIGH);  // Si el mensaje es OFF, apaga el buzzer usando lógica inversa.
    else if (messageTemp == "FAN_ON") digitalWrite(FAN_PIN, HIGH);  // Si el mensaje es FAN_ON, enciende el ventilador.
    else if (messageTemp == "FAN_OFF") digitalWrite(FAN_PIN, LOW);  // Si el mensaje es FAN_OFF, apaga el ventilador.
  }

  if (topicStr == topic_sc5_buzzer) hacerBip = true;                // Si el mensaje llega al tópico del buzzer del Escenario 5, activa la bandera para hacer un bip.
}

void setup_wifi() {                                                 // Define la función encargada de conectar el ESP32 a una red Wi-Fi disponible.
  delay(10);                                                        // Realiza una pequeña pausa para estabilizar el inicio del módulo Wi-Fi.
  Serial.println("\nInicializando búsqueda de redes Wi-Fi...");     // Muestra en el monitor serial que inicia la búsqueda de redes.
  
  WiFi.mode(WIFI_STA);                                              // Configura el ESP32 en modo estación para conectarse a un router o punto de acceso.

  // 1. Registrar red del docente (Sin contraseña)
  wifiMulti.addAP("prueba pi", "");                                // Registra la red del docente o feria, sin contraseña.
  
  // 2. Registrar red de tu casa (Con contraseña)
  wifiMulti.addAP("NETLIFE - GABRIELA", "TU_CONTRASENA_WIFI");      // Registra la red de casa; para GitHub no se debe publicar la contraseña real.

  Serial.print("Conectando");                                      // Muestra en el monitor serial que el ESP32 está intentando conectarse.
  
  // 3. El ESP32 decide a cuál conectarse automáticamente
  while (wifiMulti.run() != WL_CONNECTED) {                         // Repite el intento de conexión hasta que el ESP32 se conecte a una red registrada.
    delay(500);                                                     // Espera medio segundo entre cada intento de conexión.
    Serial.print(".");                                             // Imprime un punto para indicar que sigue intentando conectarse.
  }

  Serial.println("\n¡WiFi conectado!");                             // Muestra que la conexión Wi-Fi fue exitosa.
  Serial.print("Red actual: ");                                     // Imprime una etiqueta para mostrar el nombre de la red conectada.
  Serial.println(WiFi.SSID());                                      // Muestra el SSID de la red Wi-Fi a la que se conectó el ESP32.
  Serial.print("IP asignada al ESP32: ");                           // Imprime una etiqueta para mostrar la IP asignada.
  Serial.println(WiFi.localIP());                                   // Muestra la dirección IP local asignada al ESP32.

  // 4. Lógica de asignación de Broker según la red
  if (WiFi.SSID() == "prueba pi") {                                 // Verifica si el ESP32 se conectó a la red del docente o feria.
    mqtt_server_actual = mqtt_server_docente;                       // Asigna como broker MQTT la IP correspondiente a la red del docente.
    Serial.println("Modo Feria: Broker configurado en 192.168.1.30");// Muestra en el monitor serial que se activó el modo feria.
  } else {                                                          // Si no está conectado a la red del docente, asume que está en la red de casa.
    mqtt_server_actual = mqtt_server_casa;                          // Asigna como broker MQTT la IP correspondiente a la red de casa.
    Serial.println("Modo Casa: Broker configurado en 192.168.100.30");// Muestra en el monitor serial que se activó el modo casa.
  }
}

void reconnect_mqtt() {                                             // Define la función encargada de conectar o reconectar el ESP32 al broker MQTT.
  while (!client.connected()) {                                     // Mientras el cliente MQTT no esté conectado, intenta reconectarse.
    if (client.connect("ESP32_Aula_Sensores")) {                    // Intenta conectarse al broker MQTT con el identificador ESP32_Aula_Sensores.
      Serial.println("MQTT conectado!");                            // Muestra que la conexión MQTT fue exitosa.
      client.subscribe(mqtt_topic_sub);                             // Suscribe el ESP32 al tópico general de control.
      client.subscribe(topic_sc5_buzzer);                           // Suscribe el ESP32 al tópico específico del buzzer del Escenario 5.
      digitalWrite(BUZZER_PIN, HIGH);                               // Asegura que el buzzer quede apagado al conectarse, usando lógica inversa.
      digitalWrite(FAN_PIN, LOW);                                   // Asegura que el ventilador quede apagado al conectarse.
    } else {                                                        // Si la conexión MQTT falla, ejecuta este bloque.
      delay(2000);                                                  // Espera dos segundos antes de volver a intentar la conexión MQTT.
    }
  }
}

float getDistance() {                                               // Define una función que mide la distancia usando el sensor ultrasónico HC-SR04.
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);                // Coloca TRIG en LOW durante 2 microsegundos para asegurar un pulso limpio.
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);              // Coloca TRIG en HIGH durante 10 microsegundos para emitir el pulso ultrasónico.
  digitalWrite(TRIG_PIN, LOW);                                      // Coloca TRIG nuevamente en LOW para finalizar el pulso.
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);                   // Mide el tiempo que el pin ECHO permanece en HIGH, con un tiempo máximo de espera de 30000 microsegundos.
  return (duration == 0) ? 0 : (duration * 0.0343) / 2.0;            // Si no hay eco retorna 0; si hay eco, calcula la distancia en centímetros.
}

void setup() {                                                      // Define la función de configuración inicial, que se ejecuta una sola vez al iniciar el ESP32.
  Serial.begin(115200);                                             // Inicia la comunicación serial a 115200 baudios para depuración.
  pinMode(PIR_PIN, INPUT_PULLDOWN);                                 // Configura el pin del sensor PIR como entrada digital con resistencia pull-down interna.
  pinMode(ECHO_PIN, INPUT_PULLDOWN);                                // Configura el pin ECHO del ultrasónico como entrada digital con resistencia pull-down interna.
  pinMode(TRIG_PIN, OUTPUT);                                        // Configura el pin TRIG del ultrasónico como salida digital.
  pinMode(BUZZER_PIN, OUTPUT);                                      // Configura el pin del buzzer como salida digital.
  digitalWrite(BUZZER_PIN, HIGH);                                   // Apaga inicialmente el buzzer, debido a que trabaja con lógica inversa.
  pinMode(SEMAFORO_R_PIN, OUTPUT);                                  // Configura el pin del LED rojo como salida digital.
  pinMode(SEMAFORO_Y_PIN, OUTPUT);                                  // Configura el pin del LED amarillo como salida digital.
  pinMode(SEMAFORO_G_PIN, OUTPUT);                                  // Configura el pin del LED verde como salida digital.
  pinMode(FAN_PIN, OUTPUT);                                         // Configura el pin del ventilador como salida digital.
  digitalWrite(FAN_PIN, LOW);                                       // Apaga inicialmente el ventilador.

  // Llamamos a la configuración de red inteligente
  setup_wifi();                                                     // Ejecuta la conexión Wi-Fi automática usando las redes registradas.
  
  // Asignamos la IP dinámica del Broker al cliente MQTT
  client.setServer(mqtt_server_actual, 1883);                       // Configura el servidor MQTT seleccionado y el puerto estándar 1883.
  client.setCallback(callback);                                     // Asocia la función callback para procesar mensajes MQTT recibidos.
}

void loop() {                                                       // Define el bucle principal que se ejecuta repetidamente.
  if (!client.connected()) reconnect_mqtt();                        // Si el cliente MQTT no está conectado, llama a la función de reconexión.
  client.loop();                                                    // Mantiene activa la comunicación MQTT y permite recibir mensajes entrantes.

  if (hacerBip) {                                                   // Verifica si la bandera hacerBip está activada.
    digitalWrite(BUZZER_PIN, LOW);                                  // Enciende el buzzer usando lógica inversa.
    delay(150);                                                     // Mantiene el buzzer encendido durante 150 milisegundos.
    digitalWrite(BUZZER_PIN, HIGH);                                 // Apaga el buzzer usando lógica inversa.
    hacerBip = false;                                               // Reinicia la bandera para evitar que el buzzer siga sonando.
  }

  // --- 1. LECTURA SONIDO KY-038 (Buscador de Picos) ---
  int minVal = 4095;                                                // Inicializa el valor mínimo con el máximo posible del ADC del ESP32.
  int maxVal = 0;                                                   // Inicializa el valor máximo con el mínimo posible.
  long startMillis = millis();                                      // Guarda el tiempo actual en milisegundos para controlar la ventana de muestreo.

  while (millis() - startMillis < 50) {                             // Durante 50 milisegundos toma varias muestras del sensor de sonido.
      int lectura = analogRead(SOUND_PIN);                          // Lee el valor analógico actual del sensor de sonido.
      if (lectura < minVal) minVal = lectura;                       // Si la lectura es menor que el mínimo registrado, actualiza minVal.
      if (lectura > maxVal) maxVal = lectura;                       // Si la lectura es mayor que el máximo registrado, actualiza maxVal.
  }
  
  int amplitud_real = maxVal - minVal;                              // Calcula la amplitud real del sonido restando el mínimo al máximo.
  int sonido_para_grafica = 0;                                      // Inicializa la variable que se enviará como valor de sonido procesado.
  
  if (amplitud_real > NOISE_THRESHOLD) {                            // Verifica si la amplitud supera el umbral mínimo de ruido.
      sonido_para_grafica = map(amplitud_real, NOISE_THRESHOLD, 4095, 0, 4095); // Escala la amplitud real al rango de 0 a 4095 para graficarla.
      if (sonido_para_grafica > 4095) sonido_para_grafica = 4095;   // Limita el valor máximo para evitar que supere el rango del ADC.
  } else {                                                          // Si la amplitud no supera el umbral definido.
      sonido_para_grafica = 0;                                      // Asigna cero para considerar que no hay sonido significativo.
  }

  // --- 2. LECTURA OTROS SENSORES ---
  int luz_bruto = analogRead(LDR_PIN);                              // Lee el valor analógico bruto proveniente de la LDR.
  float luz_percent = 0;                                            // Inicializa la variable de porcentaje de luz.

  if (luz_bruto <= 10 || luz_bruto >= 4090) {                       // Verifica si la lectura está en extremos que pueden indicar ruido o desconexión.
      luz_percent = 0;                                              // Si está en extremos, asigna 0% de luz para evitar valores incorrectos.
  } else {                                                          // Si la lectura está dentro de un rango válido.
      luz_percent = 100.0 - (luz_bruto / 4095.0 * 100.0);           // Convierte el valor analógico de la LDR a porcentaje invertido de luz.
  }
  
  bool presencia = digitalRead(PIR_PIN);                            // Lee el estado digital del sensor PIR para detectar presencia o movimiento.
  float distancia = getDistance();                                  // Obtiene la distancia medida por el sensor ultrasónico llamando a getDistance().

  // --- 3. LÓGICA DEL SEMÁFORO LED ---
  digitalWrite(SEMAFORO_R_PIN, (luz_percent > 0 && luz_percent < 10.0) ? HIGH : LOW);       // Enciende el LED rojo si la luz está entre 0% y 10%.
  digitalWrite(SEMAFORO_Y_PIN, (luz_percent >= 10.0 && luz_percent < 20.0) ? HIGH : LOW);   // Enciende el LED amarillo si la luz está entre 10% y 20%.
  digitalWrite(SEMAFORO_G_PIN, (luz_percent >= 20.0 && luz_percent < 30.0) ? HIGH : LOW);   // Enciende el LED verde si la luz está entre 20% y 30%.

  // --- 4. ENVIAR DATOS (JSON ÚNICO) ---
  doc.clear();                                                       // Limpia el documento JSON para eliminar datos de ciclos anteriores.
  doc["luz_percent"] = luz_percent;                                 // Agrega al JSON el porcentaje de luz calculado.
  doc["sound_bruto"] = sonido_para_grafica;                          // Agrega al JSON el valor de sonido procesado para visualización.
  doc["sound_raw_peak"] = maxVal;                                    // Agrega al JSON el valor máximo crudo detectado por el micrófono.
  doc["presencia"] = presencia;                                      // Agrega al JSON el estado del sensor PIR.
  doc["distancia"] = distancia;                                      // Agrega al JSON la distancia medida por el sensor ultrasónico.

  char buffer[256];                                                  // Crea un arreglo de caracteres para almacenar el JSON serializado.
  serializeJson(doc, buffer);                                        // Convierte el documento JSON en texto plano y lo guarda en buffer.
  client.publish(mqtt_topic_pub, buffer);                            // Publica el JSON completo en el tópico MQTT aula/datos.

  // Publicar distancia por separado para Escenario 5
  char distStr[10];                                                  // Crea un arreglo de caracteres para almacenar la distancia como texto.
  dtostrf(distancia, 4, 1, distStr);                                 // Convierte el valor float de distancia a una cadena con un decimal.
  client.publish(topic_sc5_ultra, distStr);                          // Publica la distancia en el tópico específico del Escenario 5.
  
  delay(50);                                                         // Espera 50 milisegundos antes de repetir el ciclo principal.
}