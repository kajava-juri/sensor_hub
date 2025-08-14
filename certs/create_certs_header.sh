if [ -z "$CERTS_PATH" ]; then
    echo Define CERTS_PATH
fi

echo Generating certificates header file \"$CERTS_PATH/mqtt_client.inc\"

echo -n \#define TLS_ROOT_CERT \" > $CERTS_PATH/mqtt_client.inc
cat $CERTS_PATH/ca.crt | awk '{printf "%s\\n\\\n", $0}' >> $CERTS_PATH/mqtt_client.inc
echo "\"" >> $CERTS_PATH/mqtt_client.inc
echo >> $CERTS_PATH/mqtt_client.inc

echo -n \#define TLS_CLIENT_KEY \" >> $CERTS_PATH/mqtt_client.inc
cat $CERTS_PATH/client.key | awk '{printf "%s\\n\\\n", $0}' >> $CERTS_PATH/mqtt_client.inc
echo "\"" >> $CERTS_PATH/mqtt_client.inc
echo >> $CERTS_PATH/mqtt_client.inc

echo -n \#define TLS_CLIENT_CERT \" >> $CERTS_PATH/mqtt_client.inc
cat $CERTS_PATH/client.crt | awk '{printf "%s\\n\\\n", $0}' >> $CERTS_PATH/mqtt_client.inc
echo "\"" >> $CERTS_PATH/mqtt_client.inc
