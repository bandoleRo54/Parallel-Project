# 🖥️ Proyecto: Simulación de un ecosistema simple 🖥️
Este repositorio contiene los programas que implementan la simulación de un ecosistema con conejos, zorros y rocas, tanto en una versión secuencial y otra paralelizada mediante el uso de OpenMP, utilizando el lenguaje de programación C++.

# ¿De qué trata?
Se construyó un simulador de «ecosistema» depredador-presa en una cuadrícula R×C («mundo»), poblada inicialmente con rocas (obstáculos inamovibles), conejos (herbívoros de cría rápida) y zorros (carnívoros que cazan conejos). La simulación se ejecuta durante N_GEN «generaciones» discretas, aplicando reglas de movimiento, alimentación, reproducción, inanición y resolución de conflictos.

A nivel superficial, la arquitectura que se diseñó para ambas implementaciones se dividió de la siguiente manera:

1. **ecosystem.h**, donde se definen la interfaz y los tipos de datos a manejar.
2. **ecosystem.cpp**, donde se implementan los métodos de las estructuras de datos utilizadas.
3. **main.cpp**, donde se crea una instancia del mundo a simular, se lee la entrada, corre la simulación y por último imprime la salida.

# ¿Cómo usarlo?
- En la carpeta *src*, estarán los códigos y los ejecutables compilados, exportados directamente del contenedor de Docker en el que se construyeron.
- De acuerdo a la descripción del enunciado, la entrada conlleva multiples variables separadas por espacios (``GEN_PROC_RABBITS GEN_PROC_FOXES GEN_FOOD_FOXES N_GEN R C N``), y luego se leen $N$ lineas de $OBJETO\ X\ Y$, donde se establecen los elementos de la simulación. Todo esto se puede ajustar a gusto.
- Dado que la simulación se rige por reglas no aleatorias, una entrada debe dar siempre una salida en particular.
