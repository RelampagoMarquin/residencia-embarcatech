#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

// Função de inicialização pública que você chama na main
void mqtt_handler_init(void);

// Funções de publicação que você chama nas suas tarefas de sensor/joystick
void mqtt_handler_publish_temperature(int temp);
void mqtt_handler_publish_joystick(const char* direction);

#endif