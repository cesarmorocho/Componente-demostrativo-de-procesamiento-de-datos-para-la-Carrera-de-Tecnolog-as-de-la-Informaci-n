# Instrucciones de ejecución

## Requisitos
- Tener instalado Docker y Docker Compose.
- Si vas a ejecutar el proyecto fuera de Docker, tener Node.js y npm instalados.

## Opción recomendada: Docker
1. Abre una terminal en la carpeta raíz del proyecto.
2. Construye la imagen y levanta el contenedor:

```bash
docker compose up --build -d
```

3. Abre Node-RED en el navegador en:

```text
http://localhost:1880
```

4. Para ver los registros del contenedor:

```bash
docker compose logs -f
```

5. Para detener el entorno:

```bash
docker compose down
```

## Dependencias de Node-RED
La carpeta `node_modules` no se sube a GitHub. Se regenera desde `data/package.json` y `data/package-lock.json`.

Si necesitas reinstalar las dependencias manualmente dentro de `data`:

```bash
cd data
npm install
```

## Archivos persistentes
- Los flujos, credenciales y configuración de Node-RED se guardan en `data/`.
- No borres `data/` si quieres conservar el estado del proyecto.