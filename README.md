# **Servidor HTTP Concurrente en C**

Este proyecto implementa un **servidor HTTP multiprocesamiento** utilizando:

* **Thread Pool**
* **Shared Buffer** con la clásica solución **Productor–Consumidor**
* **Mutex y Variables de condición (`pthread_cond`)**
* **Parser HTTP propio**
* **Manejo de archivos estáticos** desde la carpeta `www/`
* **MIME types dinámicos** por extensión

El servidor soporta múltiples clientes simultáneamente sin crear un thread por conexión, logrando una arquitectura eficiente y escalable.

---

## **Arquitectura del Servidor**

```
        ┌──────────────────────────────┐
        │          main thread         │
        │     (acepta conexiones)      │
        └───────────────┬──────────────┘
                        │ PRODUCE
                        ▼
        ┌────────────────────────────────┐
        │  Buffer compartido (cola FIFO) │
        │   - Mutex                      │
        │   - cond_not_empty             │
        │   - cond_not_full              │
        └───────────────┬────────────────┘
                        │ CONSUMEN
        ┌───────────────┴──────────────┐
        │       Thread Pool (N hilos)   │
        │ handle_client(client_fd)      │
        └───────────────────────────────┘
```

### **Componentes:**

### **Productor:**

El hilo principal (`main.c`) acepta conexiones con `accept()` y las encola usando:

```c
enqueue_client(client_fd);
```

---

### **Buffer compartido (cola circular)**

Implementado con:

* `pthread_mutex_t mutex`
* `pthread_cond_t not_empty`
* `pthread_cond_t not_full`

Evita condiciones de carrera y aplica sincronización correcta usando `while` (no `if`).

---

### **Consumidores (thread pool)**

Cada hilo del pool:

1. Bloquea y hace `pop()` del buffer
2. Procesa la petición con `handle_client()`
3. Envía la respuesta HTTP
4. Cierra el socket

---

## 🌐 **Soporte de MIME Types**

El archivo `http.c` detecta automáticamente el tipo de archivo según la extensión:

| Extensión   | MIME Type                |
| ----------- | ------------------------ |
| .html, .htm | text/html                |
| .css        | text/css                 |
| .js         | application/javascript   |
| .png        | image/png                |
| .jpg, .jpeg | image/jpeg               |
| .gif        | image/gif                |
| .svg        | image/svg+xml            |
| .json       | application/json         |
| .txt        | text/plain               |
| otros       | application/octet-stream |

---

## 📁 **Estructura del Proyecto**

```text
.
├── include/              # Headers (.h)
│   ├── server.h
│   ├── http.h
│   └── parser.h
├── src/                  # Código fuente del servidor
│   ├── main.c
│   ├── server.c
│   ├── http.c
│   └── parser.c
├── test/                 # Código fuente del test de carga
│   └── angry_threads_server.c
├── obj/                  # Archivos objeto (.o)
└── bin/                  # Ejecutables
    ├── server
    └── stress_test
```

---

# **Stress Test**

El script `angry_threads_test.sh` compila y ejecuta un cliente concurrente que envía múltiples requests al servidor.

### Ejemplo:

```bash
./angry_threads_test.sh 127.0.0.1 8000 8 1000 /
```

Esto envía 8 threads × 1000 requests = **8000 peticiones**

Esto es muy útil para validar:

* El correcto funcionamiento del thread pool
* La sincronización del buffer
* La ausencia de race conditions
* El manejo de carga

## 🛠 Compilación

Asegurate de tener los directorios:

```bash
mkdir -p bin obj
```

### Servidor

Puedes usar el Makefile o también compilar el servidor con `gcc`, separando objetos y binario:

```bash
gcc -Wall -Wextra -O2 -pthread -Iinclude -c src/main.c   -o obj/main.o
gcc -Wall -Wextra -O2 -pthread -Iinclude -c src/server.c -o obj/server.o
gcc -Wall -Wextra -O2 -pthread -Iinclude -c src/http.c   -o obj/http.o
gcc -Wall -Wextra -O2 -pthread -Iinclude -c src/parser.c -o obj/parser.o

gcc -Wall -Wextra -O2 -pthread \
    obj/main.o obj/server.o obj/http.o obj/parser.o \
    -o bin/server
```


# **Ejecución**

### 1. Iniciar el servidor:

```bash
./bin/server
```

Por defecto escucha en el puerto **8000**.

---

### 2. Acceder desde navegador:

```
http://localhost:8000
```

---

### 3. O usando `curl`:

```bash
curl http://localhost:8000/index.html
```

---

# **Detalles Importantes de la Implementación**

### Thread pool fijo

No se crean threads por conexión → evita overhead.

### Buffer circular

Protegido por mutex y condition variables.

### Solución correcta a productor-consumidor

Evita:

* busy waiting
* deadlocks
* race conditions

### Parser HTTP propio

Extrae:

* método (`GET`)
* ruta solicitada (`/`, `/index.html`, `/img/logo.png`, etc.)

### Manejo de archivos binarios

Se usan:

```c
fopen(..., "rb");
fread(...);
send(...);
```
