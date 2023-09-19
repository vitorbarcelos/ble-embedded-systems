#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <zephyr.h>
#include <kernel.h>
#include <ctype.h>

typedef struct {
    struct bt_conn *connection;
} Peripheral;

static void connected(struct bt_conn *connection, uint8_t err);
static void disconnected(struct bt_conn *connection, uint8_t reason);
ssize_t receiveData(struct bt_conn *connection, const struct bt_gatt_attr *attr, const void *buffer, uint16_t length, uint16_t offset, uint8_t flags);

static Peripheral *peripheral = NULL;
static struct bt_uuid_16 uuidUpperCaseService = BT_UUID_INIT_16(0xA123);
static struct bt_uuid_16 uuidSendDataCharacteristic = BT_UUID_INIT_16(0xB123);
static struct bt_uuid_16 uuidReceiveDataCharacteristic = BT_UUID_INIT_16(0xC123);

static struct bt_gatt_attr uart_gatt_attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&uuidUpperCaseService),
    BT_GATT_CHARACTERISTIC(&uuidReceiveDataCharacteristic.uuid,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE,
                           NULL,
                           receiveData,
                           NULL),
    BT_GATT_CHARACTERISTIC(&uuidSendDataCharacteristic.uuid,
                           BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ,
                           NULL,
                           NULL,
                           NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_WRITE)};
    static struct bt_gatt_service primaryService = BT_GATT_SERVICE(uart_gatt_attrs);

static const struct bt_data AdvertisingData[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .disconnected = disconnected,
    .connected = connected
};

static void connected(struct bt_conn *connection, uint8_t err)
{
    char device[BT_ADDR_LE_STR_LEN];
    const bt_addr_le_t *destination = bt_conn_get_dst(connection);
    bt_addr_le_to_str(destination, device, sizeof(device));

    if (err)
    {
        printk("Não foi possível se conectar ao %s (%u)\n", device, err);
        return;
    }

    printk("Conectado ao %s\n", device);
    peripheral->connection = bt_conn_ref(connection);
    return;
}

static void disconnected(struct bt_conn *connection, uint8_t reason)
{
    char device[BT_ADDR_LE_STR_LEN];
    const bt_addr_le_t *destination = bt_conn_get_dst(connection);
    bt_addr_le_to_str(destination, device, sizeof(device));
    printk("Desconectado do %s (reason 0x%02x)\n", device, reason);
    peripheral->connection = NULL;
    bt_conn_unref(connection);
    return;
}

ssize_t receiveData(struct bt_conn *connection, const struct bt_gatt_attr *attr, const void *buffer, uint16_t length, uint16_t offset, uint8_t flags)
{
    if (length > 16) {
        printk("A mensagem excedeu o limite permitido.\n");
    }

    if (buffer) {
        uint8_t response[16];
        memcpy(response, buffer, length);
        printk("Uma mensagem foi recebida.\n");

        for (int i = 0; i < 16; i++)
        {
            if (isalpha(response[i]) != 0) {
                response[i] = toupper(response[i]);
            }
        }

        int status = bt_gatt_notify(peripheral->connection, &uart_gatt_attrs[2], response, length);
        if (status == 0)
            printk("Sucesso na notificação da mensagem.\n");
        else
            printk("Não foi possível notificar a mensagem.\n");
    }
    else
    {
        printk("Não foi possível notificar a mensagem.\n");
    }

    return length;
}

int startPeripheral(Peripheral *_peripheral)
{
    printk("Inicializando Bluetooth.\n");
    int status = bt_enable(NULL);
    peripheral = _peripheral;

    if (status == 0)
    {
        printk("Bluetooth inicializado.\n");
        int AdvertisingDataSize = ARRAY_SIZE(AdvertisingData);
        int status = bt_le_adv_start(BT_LE_ADV_CONN_NAME, AdvertisingData, AdvertisingDataSize, NULL, 0);

        if (status == 0)
        {
            printk("Advertising realizado.\n");
            return status;
        } else {
            printk("Não foi possível realizar o advertising.\n");
            return status;
        }
    } else {
        printk("Não foi possível inicializar o bluetooth.\n");
    }

    return status;
}

void startService()
{
    int status = bt_gatt_service_register(&primaryService);

    if (status == 0)
    {
        printk("Serviço registrado.\n");
        return;
    }

    printk("Falha no registro do serviço.\n");
    return;
}

int main()
{
    Peripheral peripheral = {
        .connection = NULL,
    };

    startPeripheral(&peripheral);
    startService();
    return 0;
}
