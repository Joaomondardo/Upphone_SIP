# Provisionamento do Upphone SIP

Estado de desenvolvimento em 15/10/2026: a branch `provisionamento` identifica o aplicativo como **1.4.0** e adiciona ativação, conta provisionada e ajustes de áudio. A versão 1.3.1 é a base publicada descrita no histórico. A presença do código novo não representa uma release 1.4.0 homologada nem comprova o atendimento integral da [especificação](ESPECIFICACAO_SOFTPHONE_UPP.md).

## Componentes

| Componente | Local e responsabilidade |
|---|---|
| Cliente de provisionamento | `Provisioning/`: HTTP, ativação, configuração, identidade do dispositivo, armazenamento e diagnóstico |
| Interface | `ActivationDlg.*`, `MyAccountDlg.*` e `AudioDlg.*`: ativação, conta e áudio |
| Integração com SIP | `settings.*` e `mainDlg.*`: aplicação do perfil e comunicação do estado local |
| Backend | Pasta irmã `upp-provisionamento`: Python/FastAPI, SQLAlchemy, Alembic e testes; destino privado autorizado [Joaomondardo/upp-provisionamento](https://github.com/Joaomondardo/upp-provisionamento) |
| Central/PBX | Responsável pelo registro observado na telefonia, chamadas, agentes e filas; a integração SyssVoIP permanece dependente de homologação |

O backend oferece `/health`, documentação `/api/docs`, ativação em `/api/v1/activation`, operações de dispositivo em `/api/v1/device/*` e gestão em `/api/v1/integration`. Neste snapshot não há módulo de painel web: `/painel/` não é uma entrega concluída. Consulte o README do backend para inicializar o serviço e gerar códigos de laboratório.

## Seleção do servidor

O código de ativação identifica o ambiente: `D` para desenvolvimento, `H` para homologação e `P` para produção. Em `ProvisioningClient::ServerFor`, a precedência do endereço é:

1. `Upphone.ini`, seção `[Provisioning]`, campo `server`.
2. Valor `ProvisioningServer` em `Software\Upphone SIP`, primeiro no registro `HKCU`, depois em `HKLM`.
3. Endereço do arquivo de ativação inicial, se presente.
4. Endereço padrão de homologação/produção definido em `define.h`.

Os domínios padrão presentes no fonte precisam ter implantação, DNS e certificados confirmados antes do uso; sua presença no código não comprova um serviço disponível. Não há endereço padrão para desenvolvimento.

Para conectar a um backend de laboratório na mesma máquina, feche o aplicativo e acrescente ao seu INI local:

```ini
[Provisioning]
server=http://127.0.0.1:8000
```

O INI fica em `%APPDATA%\Upphone SIP\` na instalação reconhecida pelo app ou junto ao executável no modo portátil. Preserve as demais seções e nunca inclua esse arquivo no Git. A exceção HTTP do cliente é limitada ao loopback; para um servidor em outra máquina, use HTTPS válido. O endpoint de provisionamento é diferente do servidor SIP cadastrado no perfil.

O cliente possui leitura de `%PROGRAMDATA%\Upphone SIP\ativacao.ini` e de valores no registro. **O instalador atual em `build/Upphone.iss` ainda não gera esse arquivo nem implementa o parâmetro `/SERVIDOR`.** A ativação automática por pacote personalizado depende de completar e homologar essa parte. Prefira inserir o código temporário na interface; não distribua senhas SIP em URLs, scripts ou pacotes.

## Persistência e estados

O perfil e a credencial do dispositivo são armazenados em `provisioning.dat`, protegido por DPAPI no contexto do usuário Windows. `provisioning.dat.tmp` é o arquivo temporário de gravação. Não copie esses arquivos como pacote de ativação para outro usuário/computador, mesmo estando cifrados.

O cliente implementa aplicação de revisão de configuração e adiamento de mudanças durante chamada, consulta periódica, informação de registro e tratamento de bloqueio/revogação. É necessário verificar o resultado no executável: resposta HTTP, configuração salva, configuração aplicada e registro SIP são estados diferentes.

O diagnóstico local é gravado em `provisioning_log.txt`, com rotação para `provisioning_log.txt.1`. O código sanitiza o conteúdo enviado, mas o material ainda pode conter identificação de dispositivos/ramais e precisa ser tratado como dado técnico restrito. Não versionar logs, credenciais ou arquivos de ativação.

O aviso de atualização e os dados de versão fornecidos pelo backend não comprovam instalação automática, verificação completa do instalador, implantação por grupos ou rollback. Bloquear a credencial de gestão também não revoga automaticamente a senha SIP na central.

## Compilação e homologação

Use o ambiente e as dependências fixadas descritos no [README](../README.md). O projeto inclui as novas fontes C++ e continua exigindo Windows, MSVC/ATL/MFC, pjproject e as bibliotecas fixadas. Compile a revisão da branch `provisionamento` em Win32 e x64; um build da tag antiga não testa as alterações novas.

Na revisão de 15/10/2026, o build **Release Win32 e x64 passou** em cópia isolada com MSVC v145, Windows SDK 10.0.26100.0 e pjproject/bcg729/SQLiteCpp nos commits fixados. O executável não foi executado nessa validação e não houve chamada real. O backend passou em 67 testes SQLite e no ciclo de migração/reversão/reaplicação; essas evidências não substituem a homologação conjunta abaixo.

Antes de distribuir uma nova release:

- Registrar commit, versão, arquitetura e SHA-256 do `.exe` e instalador efetivamente testados.
- Validar ativação, código expirado/revogado, retomada após falha, persistência após reinício e troca de usuário/computador.
- Validar aplicação de configuração durante/fora de chamada, confirmação de revisão, erro de registro, indisponibilidade do backend e reconexão.
- Testar áudio nos dois sentidos, headsets, DTMF, transferências e codecs negociados com a central, incluindo G.729.
- Cobrir Windows, rede/NAT/VPN e formas de operação de filas existentes nos clientes; usar os casos T01–T20 e FQ01–FQ12 da especificação.
- Encerrar as pendências de licença e assinatura digital antes da distribuição comercial correspondente.

Nos testes de filas SyssVoIP, separar recebimento de chamadas de fila, login/pausa de agente, monitoramento pela central, indicadores no painel UPP e supervisão de áudio. SIP registrado, DND, BLF ou heartbeat não demonstram essas capacidades. O laudo deve registrar **suportado**, **não suportado** ou **não confirmado**, com evidências do PBX e limites por perfil de cliente.

Os documentos de especificação Markdown/Word preservam o levantamento histórico da base 1.3.1. As entregas ali previstas precisam de evidência própria de implementação e aceite; esta atualização de documentação não substitui os testes de telefonia nem os testes automatizados do backend.
