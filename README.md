# Simple OTA example

Este código mostra o processo de atualização OTA. Ele inicializa um cliente HTTP com a configuração especificada e executa uma solicitação HTTP GET para recuperar um arquivo JSON contendo informações sobre a atualização do firmware. Se o arquivo JSON for recuperado com êxito, ele verificará a versão do arquivo em relação à versão atual do firmware (VERSION_APP). Se a versão recuperada for superior, inicia o download do novo firmware utilizando a função esp_https_ota. Após um download bem-sucedido, ele reinicia o ESP32 para aplicar a atualização.

# Observação

O código fornecido inclui espaços reservados para informações confidenciais, como credenciais do GitHub (YourID e YourPSSWD). Certifique-se de substituí-los pelo seu nome de usuário e senha reais do GitHub ou use um método mais seguro para autenticação. Além disso, este código pressupõe a disponibilidade de um arquivo JSON em um servidor com informações sobre a versão mais recente do firmware e o URL de download correspondente.
