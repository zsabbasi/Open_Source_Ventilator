cmake_minimum_required(VERSION 3.15)
project(ArduinoVent)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ArduinoVent
        alarm.cpp
        alarm.h
        ArduinoVent.ino
        BME280_mv.cpp
        BME280_mv.h
        bmp280_int.cpp
        bmp280_int.h
        breather.cpp
        breather.h
        breatherMotor.cpp
        config.h
        crc.cpp
        crc.h
        event.cpp
        event.h
        hal.cpp
        hal.h
        languages.h
        LcdMv.cpp
        LcdMv.h
        LiquidCrystal_I2C_mv.cpp
        LiquidCrystal_I2C_mv.h
        log.cpp
        log.h
        motor.cpp
        motor.h
        pressure.cpp
        pressure.h
        properties.cpp
        properties.h
        serialWriter.cpp
        serialWriter.h
        sSense-BMx280I2C_mv.cpp
        sSense-BMx280I2C_mv.h
        toyotaMafSensor.cpp
        toyotaMafSensor.h
        ui_native.cpp
        ui_native.h
        vent.cpp
        vent.h)
