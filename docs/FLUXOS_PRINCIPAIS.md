# O caminho dos dados

Os fluxos abaixo mostram a responsabilidade de cada parte. Consulte o [mapa dos módulos](MAPA_MODULOS.md) para localizar as telas e os arquivos de apoio.

## 1. Ativar o aplicativo

1. O usuário informa um código na tela `ActivationDlg`. O instalador também pode deixar dados de ativação para o aplicativo.
2. `ProvisioningClient::StartActivation`, em `Provisioning/Provisioning.cpp`, prepara uma chave de recuperação (`request_key`) e envia o código ao servidor. A chave permite recuperar uma resposta perdida sem simplesmente criar outra ativação.
3. A rota `POST /api/v1/activation`, em `app/api/device.py` do backend, chama `services/provisioning.py:activate` para conferir o código e tratar o dispositivo.
4. O servidor devolve a credencial do dispositivo e a configuração. `HandleActivation` e `ApplyConfig` conferem a resposta no aplicativo.
5. O aplicativo grava seu estado em `provisioning.dat`, protegido pelo Windows, e aplica a conta. O registro do ramal na central é outra etapa: ativar na Upp não garante que o SIP registrou.

As requisições de rede usam uma thread de trabalho. O resultado volta para a thread da interface, responsável por atualizar telas e coordenar a aplicação da configuração. Veja `WorkerLoop` e `OnResult`.

## 2. Receber uma configuração nova

```mermaid
flowchart LR
    A[Painel ou API altera dados] --> B[Backend calcula configuração e revisão]
    B --> C[Aplicativo recebe a identificação pelo status]
    C --> D[Aplicativo busca a configuração]
    D --> E[Valida e grava]
    E --> F[Aplica agora ou aguarda o fim da chamada]
    F --> G[Informa o resultado ao backend]
```

No backend, `current_config` calcula o identificador do conteúdo e registra uma nova revisão quando necessário. No aplicativo, `SendStatus` envia o estado; `HandleStatus`, `FetchConfig` e `HandleConfig` coordenam a busca. `ApplyConfig` recusa uma configuração inválida ou de outro ambiente.

Se houver chamada e a mudança exigir reiniciar conta, codecs de áudio ou comunicação SIP, o aplicativo guarda a configuração como pendente. `OnCallsEnded` volta a tentar aplicá-la. A revisão só deve ser informada como aplicada depois que a gravação protegida e a aplicação forem aceitas. Veja também [ordem das configurações](ORDEM_CONFIGURACOES.md).

## 3. Guardar contatos e histórico de chamadas

As telas ficam em `Contacts.cpp` e `Calls.cpp`. As regras e operações de importação/exportação ficam em `Service/ContactsService.cpp` e `Service/CallsService.cpp`. Os repositórios em `Data/` executam operações no banco SQLite.

Pense nas camadas assim: **tela** mostra e recebe dados; **serviço** organiza operações; **repositório** lê e grava no banco. Existem caminhos legados na aplicação: confira a função envolvida antes de assumir que toda operação percorre as três camadas.

`Utils/Xml.cpp` trata a estrutura XML usada por contatos/diretório. O banco e o XML são formatos diferentes; importar ou exportar não significa que todo contato seja armazenado apenas em XML.

## 4. Atualizar o aplicativo

1. `services/provisioning.py:update_offer` escolhe a versão oferecida considerando cliente, canal, distribuição gradual, horário permitido e versão mínima. Também escolhe os dados do instalador de 32 ou 64 bits.
2. O aplicativo recebe a oferta, incluindo endereço, versão e SHA-256, um resumo matemático usado para conferir o conteúdo do arquivo.
3. `Provisioning/UpdateDownload.cpp:DownloadUpdate` valida o endereço, baixa com limite de tamanho, compara SHA-256 e confere a versão do executável.
4. A função verifica a assinatura conforme a política de `_GLOBAL_UPDATE_PUBLISHER`. Na implementação examinada, um instalador sem assinatura pode ser aceito se esse nome estiver vazio e o arquivo não tiver assinatura embutida. Não descreva isso como exigência universal de instalador assinado.
5. `LaunchUpdate` inicia o instalador com os argumentos de atualização. Testar o download isoladamente não comprova que a instalação foi concluída.
