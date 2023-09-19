#include <console/console.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <zephyr.h>
#include <kernel.h>

typedef struct {
    // Enable
    // -- 0 Disabled
    // -- 1 Enabled
    unsigned short int enable;
    struct bt_conn *connection;
    uint16_t handle;
} Central;

static int gattSubscribe();
static int startGattDiscover();
static void connected(struct bt_conn *connection, uint8_t err);
static void disconnected(struct bt_conn *connection, uint8_t reason);
static void write(struct bt_conn *connection, uint8_t err, struct bt_gatt_write_params *params);
static uint8_t notify(struct bt_conn *connection, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length);
static uint8_t gattDiscoverCallback(struct bt_conn *connection, const struct bt_gatt_attr *attribute, struct bt_gatt_discover_params *params);

static Central *central = NULL;
static struct bt_gatt_discover_params discoverParams;
static struct bt_uuid_16 uuidUpperCaseService = BT_UUID_INIT_16(0xAB01);
static struct bt_uuid_16 uuidReceiveDataCharacteristic = BT_UUID_INIT_16(0xAB02);

static struct bt_gatt_subscribe_params subscriptionParams = {
    .end_handle = BT_ATT_LAST_ATTTRIBUTE_HANDLE,
    .disc_params = &discoverParams,
    .value = BT_GATT_CCC_NOTIFY,
    .notify = notify,
    .ccc_handle = 0, 
    .write = write
};

static struct bt_gatt_discover_params discoverParams = {
    .start_handle = BT_ATT_FIRST_ATTTRIBUTE_HANDLE,
    .end_handle = BT_ATT_LAST_ATTTRIBUTE_HANDLE,
    .uuid = &uuidUpperCaseService.uuid,
    .type = BT_GATT_DISCOVER_PRIMARY,
    .func = gattDiscoverCallback,
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
        printk("Não foi possível se conectar ao %s.\n", device);
        return;
    }

    printk("Conectado ao %s.\n", device);
    central->connection = bt_conn_ref(connection);
    startGattDiscover();

    return;
}

static void disconnected(struct bt_conn *connection, uint8_t reason)
{
    char device[BT_ADDR_LE_STR_LEN];
    const bt_addr_le_t *destination = bt_conn_get_dst(connection);
    bt_addr_le_to_str(destination, device, sizeof(device));
    printk("Desconectado do %s (reason 0x%02x).\n", device, reason);
    central->connection = NULL;
    bt_conn_unref(connection);
    return;
}

static void afterDeviceFound(const bt_addr_le_t *address, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buffer) {
    char device[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(address, device, BT_ADDR_LE_STR_LEN);

    if (bt_le_scan_stop() == 0) {
        int status = bt_conn_le_create(address, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &central->connection);
        if (status == 0) {
            printk("Iniciando conexão com o %s.\n", device);
            return;
        }
    }
    
    printk("Não foi possível se conectar com o %s.\n", device);
    return;
}

static uint8_t gattDiscoverCallback(struct bt_conn *connection, const struct bt_gatt_attr *attribute, struct bt_gatt_discover_params *params) {
    struct bt_gatt_chrc *user = attribute->user_data;

    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        if (bt_uuid_cmp(user->uuid, &uuidReceiveDataCharacteristic.uuid) == 0) {
            central->handle = user->value_handle;
            return 1;
        }
    }

    if (bt_uuid_cmp(params->uuid, &uuidUpperCaseService.uuid) == 0) {
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        params->start_handle = attribute->handle + 1;
        params->uuid = NULL;
        bt_gatt_discover(connection, params);
        return 0;
    }

    gattSubscribe();
    return 0;
}

static uint8_t notify(struct bt_conn *connection, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{    
    if (length > 32) {
        printk("A mensagem recebida excedeu o limite permitido.\n");
        return 1;
    }

    char response[32];
    memcpy(response, data, length);
    response[length] = '\0';
    printk("Recebida: %s\n", (char *) response);
    return 1;
}

static void write(struct bt_conn *connection, uint8_t err, struct bt_gatt_write_params *params) {
    if (err == 0) {
        printk("Subscrição de escrita com sucesso.\n");
        return;
    }

    printk("Aconteceu um erro na subscrição de escrita.\n");
    return;
}

int startCentral(Central *_central)
{
    printk("Inicializando Bluetooth.\n");
    int status = bt_enable(NULL);
    central = _central;

    if (status == 0)
    {
        central->enable = 1;
        printk("Bluetooth inicializado.\n");
        int status = bt_le_scan_start(BT_LE_SCAN_PASSIVE, afterDeviceFound);

        if (status == 0)
        {
            printk("Escaneamento realizado.\n");
            return status;
        } else {
            printk("Não foi possível realizar o escaneamento.\n");
            return status;
        }
    } else {
        printk("Não foi possível realizar o escaneamento.\n");
        central->enable = 0;
    }

    return status;
}

static int gattSubscribe() {
    subscriptionParams.value_handle = central->handle;
    int status = bt_gatt_subscribe(central->connection, &subscriptionParams);

    if (status == 0) {
        printk("Subscrição de leitura com sucesso.\n");
        return status;
    }

    printk("Aconteceu um erro na subscrição de leitura.\n");
    return status;
}

static int startGattDiscover() {
    int status = bt_gatt_discover(central->connection, &discoverParams);
    
    if (status == 0) {
        printk("Iniciando descobertas de serviços.\n");
        return status;
    }

    printk("Não foi possível iniciar a descobertas de serviços.\n");
    return status;
}

static void sendMessage(char *message) {
    size_t length = strlen(message);
    int status = bt_gatt_write_without_response(central->connection, central->handle, message, length, false);
    
    if (status == 0) {
        printk("Mensagem enviada.\n");
        k_sleep(K_MSEC(250));
        return;
    }

    printk("Não foi possível enviar a mensagem.\n");
    return;
}

static void initSendMessage() {
    console_getline_init();
    k_sleep(K_MSEC(800));
    while (1) {
        printk("Enviar: ");
        char *line = console_getline();
        sendMessage(line);
    }
}

int main()
{
    Central central = {
        .connection = NULL,
        .enable = 0
    };

    startCentral(&central);
    initSendMessage();
    return 0;
}
