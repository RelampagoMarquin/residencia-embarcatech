#ifndef MPU6050_H
#define MPU6050_H

#include "hardware/i2c.h"
#include "pico/stdlib.h"

// Endereço I2C do MPU-6050
static const uint8_t MPU6050_ADDR = 0x68;

// Registradores do MPU-6050
static const uint8_t MPU6050_ACCEL_XOUT_H = 0x3B;
static const uint8_t MPU6050_GYRO_XOUT_H = 0x43;
static const uint8_t MPU6050_TEMP_OUT_H = 0x41;
static const uint8_t MPU6050_PWR_MGMT_1 = 0x6B;
static const uint8_t MPU6050_WHO_AM_I = 0x75;

// Estrutura para armazenar os dados do sensor
typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float temp;
} mpu_data_t;

// Protótipos das Funções
void mpu6050_reset(i2c_inst_t *i2c) {
    uint8_t buf[] = {MPU6050_PWR_MGMT_1, 0x00};
    i2c_write_blocking(i2c, MPU6050_ADDR, buf, 2, false);
}

void mpu6050_read_raw(i2c_inst_t *i2c, int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[6];

    // Lê acelerômetro
    uint8_t val = MPU6050_ACCEL_XOUT_H;
    i2c_write_blocking(i2c, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(i2c, MPU6050_ADDR, buffer, 6, false);
    for (int i = 0; i < 3; i++) {
        accel[i] = (buffer[i * 2] << 8 | buffer[i * 2 + 1]);
    }

    // Lê giroscópio
    val = MPU6050_GYRO_XOUT_H;
    i2c_write_blocking(i2c, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(i2c, MPU6050_ADDR, buffer, 6, false);
    for (int i = 0; i < 3; i++) {
        gyro[i] = (buffer[i * 2] << 8 | buffer[i * 2 + 1]);
    }

    // Lê temperatura
    val = MPU6050_TEMP_OUT_H;
    i2c_write_blocking(i2c, MPU6050_ADDR, &val, 1, true);
    i2c_read_blocking(i2c, MPU6050_ADDR, buffer, 2, false);
    *temp = buffer[0] << 8 | buffer[1];
}

void mpu6050_get_data(i2c_inst_t *i2c, mpu_data_t *data) {
    int16_t acceleration[3], gyro[3], temp_raw;
    mpu6050_read_raw(i2c, acceleration, gyro, &temp_raw);

    // Converte os valores brutos para unidades físicas
    // Fator de escala para AFS_SEL=0 (±2g) -> 16384.0
    data->accel_x = acceleration[0] / 16384.0f;
    data->accel_y = acceleration[1] / 16384.0f;
    data->accel_z = acceleration[2] / 16384.0f;

    // Fator de escala para FS_SEL=0 (±250°/s) -> 131.0
    data->gyro_x = gyro[0] / 131.0f;
    data->gyro_y = gyro[1] / 131.0f;
    data->gyro_z = gyro[2] / 131.0f;

    // Converte temperatura para Celsius
    data->temp = (temp_raw / 340.0f) + 36.53f;
}

#endif