# Como aplicativo e servidor conversam

Um contrato de comunicação define endereço, método HTTP, dados aceitos e resposta. As referências atuais são `upp-provisionamento/app/api/device.py` e `services/provisioning.py`; no aplicativo, `Provisioning/Provisioning.cpp` e `ProvisioningHttp.cpp`. Os modelos completos podem ser consultados no código e, com o backend rodando, em `/api/docs` e `/api/openapi.json`.

## Operações da API do aplicativo

Todos os caminhos abaixo começam com `/api/v1`.

| Método e caminho | Para que serve | Resposta de sucesso |
| --- | --- | --- |
| `POST /activation/check` | Confere o código para o instalador, sem consumi-lo | `200`, identificação e validade |
| `POST /activation` | Ativa o dispositivo com código, informações da máquina e chave de recuperação | `201`, credencial e configuração |
| `GET /device/config` | Busca a configuração desejada | `200`, dados; ou `304`, se o identificador já for o mesmo |
| `POST /device/status` | Envia estado do aplicativo e recebe orientações | `200`, revisão desejada, ações e oferta de versão |
| `POST /device/logs` | Envia texto de diagnóstico, sujeito a limite de tamanho e limpeza de segredos | `201`, identificação do registro |
| `POST /device/logout` | Informa a saída da conta | `204`, sem conteúdo |

As rotas `/device/*` autenticam o dispositivo com `Authorization: Bearer <token>` e `X-Upp-Machine: <identidade da máquina>`. Ativação usa o código e tem verificações próprias. Token e senha SIP têm funções distintas: o primeiro acessa a API da Upp; a segunda autentica o ramal na central. Nunca inclua os valores reais em documentação ou exemplos.

## Configuração e confirmação

O documento `config` usa `schema = 1` na versão examinada. Esse número descreve o formato, não a versão do executável. Ele contém grupos como `account`, `settings`, `management`, `client`, `user` e `support`. `account` inclui a senha SIP; a resposta requer cuidado com logs e cache.

`config_revision` é o número da revisão. `config_etag` identifica o conteúdo. O servidor calcula esse identificador com HMAC, que usa uma chave secreta, em vez de um hash simples da configuração com senha. `GET /device/config` aceita `If-None-Match`; se ele corresponder ao ETag atual, responde `304`.

O aplicativo envia no status a revisão, o ETag e um dos estados `applied` (aplicado), `pending` (pendente) ou `rejected` (recusado). Também informa registro SIP e se há chamada. A resposta de status normalmente não entrega o documento de configuração: indica a revisão desejada para que o aplicativo busque quando necessário. Ela pode incluir `rotate_token`, uma nova credencial que substitui a anterior conforme as regras do servidor e do aplicativo.

## Falhas e limites

Erros de serviço são tratados em `app/main.py` e usam campos como `error`, `message` e `request_id`. O último ajuda a relacionar uma resposta ao registro da requisição. A validação de dados também pode responder `422`; um envio de log grande demais responde `413`.

Em `HandleCommonErrors`, o aplicativo distingue revogação/saída/identidade incorreta (`401`) de bloqueios de serviço (`403`). `429` indica excesso de requisições. Falha de rede pode nem produzir uma resposta HTTP: a camada C++ usa status interno zero para esse caso. Consulte a função responsável antes de decidir como repetir uma operação.

A API de integração em `app/api/integration.py` serve à gestão por outros sistemas e usa credencial própria. O painel usa sessão e proteção de formulários. Não reutilize o token do dispositivo como se fosse uma credencial de administrador.
