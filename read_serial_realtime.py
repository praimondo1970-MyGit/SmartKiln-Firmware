import serial
import time
import sys
from datetime import datetime

try:
    print('=' * 70)
    print('MONITOR SERIAL ESP32 - Capturando eventos en tiempo real')
    print('Esperando eventos... (Presiona Ctrl+C para detener)')
    print('=' * 70)
    
    ser = serial.Serial('COM5', 115200, timeout=1)
    
    buffer = ""
    line_count = 0
    
    while True:
        try:
            # Leer datos disponibles
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data
                
                # Procesar líneas completas
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if line:
                        timestamp = datetime.now().strftime('%H:%M:%S')
                        
                        # Mostrar todas las líneas, pero destacar eventos importantes
                        if any(keyword in line for keyword in [
                            '[BLE]', 'LOAD_PROFILE', 'PROFILE', 'Curva recibida',
                            'CONECTADO', 'DESCONECTADO', 'dispositivo', 'Dispositivos conectados',
                            'WiFi suspendido', 'WiFi reactivado', 'BLE', 'Error'
                        ]):
                            print(f"\n[{timestamp}] >>> {line}")
                        else:
                            print(f"[{timestamp}] {line}")
                        
                        line_count += 1
                        
            else:
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print('\n\nDeteniendo monitor...')
            break
        except Exception as e:
            print(f'\nError: {e}')
            break
    
    ser.close()
    print(f'\nTotal de líneas capturadas: {line_count}')
    
except serial.SerialException as e:
    print(f'Error de conexión serial: {e}')
except Exception as e:
    print(f'Error: {e}')
    import traceback
    traceback.print_exc()




























