/* 
 * Bluetooth Programmer pair to be attached on the mouse side
 * Acts as a BLE Peripheral and transmits swo, sck, and tx, rx signals
 */
#include "mbed.h"
#include "ble/BLE.h"


#define BLE_UUID_TXRX_SERVICE            0x0000 /**< The UUID of the Nordic UART Service. */
#define BLE_UUID_TX_CHARACTERISTIC       0x0002 /**< The UUID of the TX Characteristic. */
#define BLE_UUIDS_RX_CHARACTERISTIC      0x0003 /**< The UUID of the RX Characteristic. */

#define TXRX_BUF_LEN                     20

BLE  ble;

Serial pc(USBTX, USBRX);

DigitalOut led(LED1);


// The Nordic UART Service
static const uint8_t uart_base_uuid[] = {0x71, 0x3D, 0, 0, 0x50, 0x3E, 0x4C, 0x75, 0xBA, 0x94, 0x31, 0x48, 0xF1, 0x8D, 0x94, 0x1E};
static const uint8_t uart_tx_uuid[]   = {0x71, 0x3D, 0, 3, 0x50, 0x3E, 0x4C, 0x75, 0xBA, 0x94, 0x31, 0x48, 0xF1, 0x8D, 0x94, 0x1E};
static const uint8_t uart_rx_uuid[]   = {0x71, 0x3D, 0, 2, 0x50, 0x3E, 0x4C, 0x75, 0xBA, 0x94, 0x31, 0x48, 0xF1, 0x8D, 0x94, 0x1E};
static const uint8_t uart_base_uuid_rev[] = {0x1E, 0x94, 0x8D, 0xF1, 0x48, 0x31, 0x94, 0xBA, 0x75, 0x4C, 0x3E, 0x50, 0, 0, 0x3D, 0x71};


uint8_t txPayload[TXRX_BUF_LEN] = {0,};
uint8_t rxPayload[TXRX_BUF_LEN] = {0,};

static uint8_t rx_buf[TXRX_BUF_LEN];
static uint8_t rx_len=0;


GattCharacteristic  txCharacteristic (uart_tx_uuid, txPayload, 1, TXRX_BUF_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
                                      
GattCharacteristic  rxCharacteristic (uart_rx_uuid, rxPayload, 1, TXRX_BUF_LEN, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);
                                      
GattCharacteristic *uartChars[] = {&txCharacteristic, &rxCharacteristic};

GattService         uartService(uart_base_uuid, uartChars, sizeof(uartChars) / sizeof(GattCharacteristic *));



void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    pc.printf("Disconnected \r\n");
    pc.printf("Restart advertising \r\n");
    ble.startAdvertising();
}

void WrittenHandler(const GattWriteCallbackParams *Handler)
{   
    uint8_t buf[TXRX_BUF_LEN];
    uint16_t bytesRead, index;
    
    if (Handler->handle == txCharacteristic.getValueAttribute().getHandle()) 
    {
        ble.readCharacteristicValue(txCharacteristic.getValueAttribute().getHandle(), buf, &bytesRead);
        memset(txPayload, 0, TXRX_BUF_LEN);
        memcpy(txPayload, buf, TXRX_BUF_LEN);       
        pc.printf("WriteHandler \r\n");
        pc.printf("Length: ");
        pc.putc(bytesRead);
        pc.printf("\r\n");
        pc.printf("Data: ");
        for(index=0; index<bytesRead; index++)
        {
            pc.putc(txPayload[index]);        
        }
        pc.printf("\r\n");
    }
}

void uartCB(void)
{   
    while(pc.readable())    
    {
        rx_buf[rx_len++] = pc.getc();    
        if(rx_len>=20 || rx_buf[rx_len-1]=='\0' || rx_buf[rx_len-1]=='\n')
        {
            ble.updateCharacteristicValue(rxCharacteristic.getValueAttribute().getHandle(), rx_buf, rx_len); 
            pc.printf("RecHandler \r\n");
            pc.printf("Length: ");
            pc.putc(rx_len);
            pc.printf("\r\n");
            rx_len = 0;
            break;
        }
    }
}

int main(void)
{
   ble.init();
   ble.onDisconnection(disconnectionCallback);
   ble.onDataWritten(WrittenHandler);  
//    
    pc.baud(115200);
   pc.printf("SimpleChat Init \r\n");
   
   pc.attach( uartCB , pc.RxIrq);
  // setup advertising 
   ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED);
   ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
   ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                   (const uint8_t *)"Biscuit", sizeof("Biscuit") - 1);
   ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,
                                   (const uint8_t *)uart_base_uuid_rev, sizeof(uart_base_uuid));
   // 100ms; in multiples of 0.625ms. 
   ble.setAdvertisingInterval(160);

   ble.addService(uartService);
   
   ble.startAdvertising(); 
   pc.printf("Advertising Start \r\n");
    
    while(1)
    {
       ble.waitForEvent(); 
        led = 1;
        wait(.5);
        led = 0;
        wait(.5);
        pc.printf("hi\n");
    }
}
















