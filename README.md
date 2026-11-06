# Sensore Garage

Firmware PlatformIO per ESP-01 1 MB, DHT22 e MQTT.

## Configurazione locale

Creare `lib/secrets.h` partendo da `lib/secrets.example.h` e compilare i valori
Wi-Fi, MQTT e OTA. Il file locale viene escluso da Git.

L'aggiornamento web su `http://<IP-del-dispositivo>/` usa:

- utente: `secrets::MQTT_CLIENT_NAME`;
- password: `secrets::OTA_PASSWORD`.

La password OTA viene letta dallo stesso file anche per gli upload PlatformIO.
Mantenere `OTA_PASSWORD` come singola stringa tra doppi apici su una riga;
per eventuali caratteri speciali usare escape JSON (`\"`, `\\`, ecc.).
I messaggi di debug EspMQTTClient sono disabilitati perché espongono
la password web sulla seriale.

## Compilazione e aggiornamento

```text
pio run
pio run -t upload --upload-port <IP-del-dispositivo>
pio device monitor
```

L'IP statico configurato in `src/main.cpp` e l'indirizzo `upload_port` in
`platformio.ini` sono entrambi `192.168.1.36`.
Se il firmware già installato usa una password diversa,
il primo aggiornamento va eseguito dalla pagina web oppure via seriale.

## Funzionamento

Il sensore viene letto ogni 10 secondi. La telemetria viene pubblicata ogni
60 secondi e usa la media degli ultimi tre campioni validi disponibili.
Una lettura non valida azzera la finestra e sospende la telemetria fino alla
successiva lettura valida. Sono ammessi anche temperatura e punto di rugiada
pari a zero. Topic e struttura dei messaggi MQTT restano quelli originali.

Il progetto attivo è `src/main.cpp`; la cartella `GiFe_Garage_1_2024_12_03`
contiene la versione Arduino precedente e non viene compilata da PlatformIO.
GPIO0 e GPIO2 sono pin di boot dell'ESP-01: il loro cablaggio va verificato
sull'hardware. Il pin GPIO2 mantiene il comportamento LOW del firmware originale.

I segreti sono esclusi dai nuovi commit, ma le versioni storiche del firmware
possono ancora contenere le vecchie credenziali. Cambiare le credenziali se
il repository o i precedenti binari sono stati condivisi.
