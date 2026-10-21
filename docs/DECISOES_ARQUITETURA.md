# Por que o sistema funciona assim?

Estas notas explicam escolhas visíveis na implementação examinada em 20/10/2026. Elas ajudam a entender o código; não substituem evidências de teste nem registram como confirmada uma motivação histórica que não foi documentada.

## Aplicativo e servidor separados

O aplicativo lida com interface Windows e telefonia. O backend mantém cadastros, permissões e configurações e oferece o painel web. Separar essas responsabilidades permite administrar vários computadores pelo servidor, enquanto a central telefônica continua encarregada das chamadas. Veja `Provisioning/` no aplicativo e `app/api/` e `app/services/` no backend.

## Proteção local com o Windows

O estado provisionado fica em `provisioning.dat`, protegido pela DPAPI, um serviço do Windows que cifra dados ligados ao contexto do usuário. Isso evita guardar a senha SIP como texto simples nesse arquivo. Também explica por que copiar o arquivo para outro usuário/computador não é uma estratégia de instalação. Veja `ProvisioningSupport.cpp` e o código de gravação do estado.

## Rede fora da thread da janela

Uma resposta HTTP pode demorar. `WorkerLoop` executa o trabalho de rede; `OnResult` trata o resultado na parte que coordena a interface. Assim, a espera de rede não precisa bloquear a janela. Ao mudar esse fluxo, preserve quem é responsável por cada estado compartilhado. Veja `ProvisioningClient` e `ProvJob`.

## Esperar a chamada terminar

Trocar conta, codecs ou parâmetros globais SIP pode reiniciar recursos usados pela chamada. `ApplyConfig` guarda a configuração pendente e `OnCallsEnded` tenta aplicá-la quando não há mais chamadas. Não basta atualizar uma variável: é preciso preservar a chamada e informar ao servidor qual revisão realmente está em uso.

## HTTPS, SIP/TLS e SRTP são proteções diferentes

HTTPS protege a comunicação com a API. SIP sobre TLS protege a sinalização com a central. SRTP protege a mídia, como áudio. Um deles não comprova que os outros estejam funcionando. O projeto configura SIP/TLS com Schannel, a implementação TLS do Windows, e usa SDES para trocar chaves SRTP. Veja `build/pjproject/config_site.h` e o patch `schannel-rand-bytes.patch`, necessário para gerar bytes aleatórios seguros.

## Revisão e identificador de conteúdo

O número de revisão ajuda a acompanhar mudanças. O ETag permite comparar conteúdos; no backend ele usa HMAC por envolver dados com senha. O aplicativo informa se a revisão foi aplicada, ficou pendente ou foi recusada. Assim, o painel pode distinguir configuração desejada de configuração informada pelo computador. Veja `current_config`, `config_etag` e `SendStatus`.

## Graphify como ferramenta de navegação

O Graphify cria um índice local das relações encontradas no código. Ele ajuda a localizar funções e classes antes da leitura detalhada. Macros MFC, eventos e comportamento de rede exigem conferência nos fontes e testes. Atualizar o índice após um conjunto de mudanças evita reconstruções a cada edição. Documentos são lidos por assunto para manter o contexto da IA pequeno; o histórico permanece disponível.
