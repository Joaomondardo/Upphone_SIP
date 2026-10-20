# Provisionamento do Upphone SIP

Estado de desenvolvimento em 15/10/2026: a branch `provisionamento` identifica o aplicativo como **1.4.0** e adiciona ativação, conta provisionada e ajustes de áudio. A versão 1.3.1 é a base publicada descrita no histórico. A presença do código novo não representa uma release 1.4.0 homologada nem comprova o atendimento integral da [especificação](ESPECIFICACAO_SOFTPHONE_UPP.md).

## Componentes

| Componente | Local e responsabilidade |
| --- | --- |
| Cliente de provisionamento | `Provisioning/`: HTTP, ativação, configuração, identidade do dispositivo, armazenamento e diagnóstico |
| Interface | `ActivationDlg.*`, `MyAccountDlg.*` e `AudioDlg.*`: ativação, conta e áudio |
| Integração com SIP | `settings.*` e `mainDlg.*`: aplicação do perfil e comunicação do estado local |
| Backend | Pasta irmã `upp-provisionamento`: Python/FastAPI, SQLAlchemy, Alembic e testes; destino privado autorizado [Joaomondardo/upp-provisionamento](https://github.com/Joaomondardo/upp-provisionamento) |
| Central/PBX | Responsável pelo registro observado na telefonia, chamadas, agentes e filas; a integração SyssVoIP permanece dependente de homologação |

O backend oferece `/health`, documentação `/api/docs`, ativação em `/api/v1/activation`, operações de dispositivo em `/api/v1/device/*` e gestão em `/api/v1/integration`. O painel web da Upp fica em `/painel/` (login com segundo fator): cadastros, códigos de ativação, dispositivos, bloqueios, logs, eventos, auditoria, versões e integrações. O papel Gestor do cliente opera só os dados do próprio cliente. Consulte o README do backend para inicializar o serviço e `deploy/README.md` para homologação e produção.

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

O cliente lê `%PROGRAMDATA%\Upphone SIP\ativacao.ini` (seção `Ativacao`: `codigo`, `servidor`, `usuario`) e o valor `ProvisioningServer` do registro. O instalador (`build/Upphone.iss`) gera esses dados:

- No assistente, a página **Ativação** pede o código (opcional), confere o dígito verificador e consulta `POST /api/v1/activation/check` no servidor do ambiente, sem usar o código. Código recusado ou expirado impede avançar; sem código, o app pede na primeira abertura.
- Instalação silenciosa: `Upphone-SIP-<versão>-x64-setup.exe /VERYSILENT /CODIGO=XXXX-XXXX-XXXX-XXXX`. `/SERVIDOR=https://...` troca o servidor (obrigatório para códigos de desenvolvimento `D`; só HTTPS ou HTTP no próprio computador). `/USUARIO=DOMINIO\usuario` restringe o código a um usuário do Windows. Em modo silencioso o código não é conferido no servidor.
- O código vai para o `ativacao.ini` (pasta com permissão de alteração para os usuários) e o servidor para `HKLM\Software\Upphone SIP\ProvisioningServer`. Quem ativa é o app, ao abrir com o usuário final; depois de usar o código ele apaga o `ativacao.ini`. O desinstalador remove o `ativacao.ini`.

- **Cliente que já usa o Upphone com conta manual:** atualizar para a 1.4.0 (assistente sem código) mantém a conta, a senha cifrada, os contatos e o histórico, e o app não pede ativação. Se depois ativar o provisionamento, a conta manual é apagada (para a senha antiga não ficar guardada) e não volta ao sair da conta.
- Em computador **já ativado**, o código do instalador não troca o ramal: fica guardado e aparece preenchido na tela de ativação se o usuário sair da conta (ou se a Upp desconectar o computador), esperando o "Ativar".
- Instalação silenciosa feita por outra conta ou pela sessão de serviço (ferramenta de distribuição da TI) não consegue fechar o Upphone aberto na sessão do usuário e termina com o código 5: feche o app antes (`Upphone.exe /exit` na sessão do usuário) ou instale com o usuário fora da sessão.
- A desinstalação remove o programa, `ativacao.ini`, o registro da máquina e, feita pelo próprio usuário, a inicialização automática e os protocolos `tel:`/`sip:`/`callto:` dele. Os dados do usuário em `%APPDATA%\Upphone SIP` ficam (contatos, histórico, configuração e a credencial cifrada `provisioning.dat`): reinstalar volta no mesmo ramal. Para retirar o computador de vez, use "Sair da conta" antes ou desconecte-o no painel.

O código é temporário e vale para um computador só; não distribua senhas SIP em URLs, scripts ou pacotes.

**Testado em 19/10/2026 numa VM Windows Server 2022 (VirtualBox, UAC ligado, usuário administrador com aprovação), com o backend de desenvolvimento:** assistente com a página Ativação (código com dígito errado recusado; código em minúsculas e com espaços aceito e conferido no servidor), `ativacao.ini`, permissões e registro gravados, app aberto pelo "Concluir" ativando sozinho e apagando o `ativacao.ini`; janelas Ativação, Minha conta e Áudio visíveis com a principal minimizada; instalação silenciosa com `/CODIGO`, `/SERVIDOR` e `/USUARIO`; desinstalação silenciosa e pelo usuário. O registro SIP não entrou nesse teste (o PABX de teste roda no Docker, desligado durante o uso da VM).

## Persistência e estados

O perfil e a credencial do dispositivo são armazenados em `provisioning.dat`, protegido por DPAPI no contexto do usuário Windows. `provisioning.dat.tmp` é o arquivo temporário de gravação. Não copie esses arquivos como pacote de ativação para outro usuário/computador, mesmo estando cifrados.

O cliente implementa aplicação de revisão de configuração e adiamento de mudanças durante chamada, consulta periódica, informação de registro e tratamento de bloqueio/revogação. É necessário verificar o resultado no executável: resposta HTTP, configuração salva, configuração aplicada e registro SIP são estados diferentes.

O diagnóstico local é gravado em `provisioning_log.txt`, com rotação para `provisioning_log.txt.1`. O código sanitiza o conteúdo enviado, mas o material ainda pode conter identificação de dispositivos/ramais e precisa ser tratado como dado técnico restrito. Não versionar logs, credenciais ou arquivos de ativação.

**Atualizações (RF16, RN09).** No painel, cada versão tem o endereço e o SHA-256 do instalador de 64 e de 32 bits (cada computador recebe os da sua arquitetura), canal (piloto ou estável), situação (distribuindo, pausada, retirada) e **distribuição gradual** (porcentagem dos computadores do canal; cada computador tem um sorteio fixo por versão, então aumentar a porcentagem só inclui computadores). Cada cliente tem canal, versão fixada (reversão controlada) e **janela de manutenção** (`HH:MM-HH:MM`, horário de Brasília, pode passar da meia-noite). Fora da janela ou do sorteio a oferta não aparece. Abaixo da versão mínima do ambiente a versão mais nova é oferecida sempre, e o aviso "Atualização necessária" aparece ao abrir o app e a cada hora (fora de chamadas), qualquer que seja o intervalo de busca escolhido pelo usuário; o "Não" não é memorizado.

No aviso, o "Sim" de uma versão oferecida pela Upp não abre mais o navegador: o app baixa o instalador por HTTPS (certificado validado; o download não leva credenciais e segue até 5 redirecionamentos, como o do GitHub Releases, cada destino aceito só em HTTPS: o WinINet seguia sozinho um redirecionamento para HTTP, achado em teste) e só executa depois de conferir: (1) o SHA-256 cadastrado no painel (obrigatório; sem ele nada é executado), (2) a versão gravada no instalador igual à oferecida e (3) a assinatura Authenticode: arquivo assinado precisa ter assinatura válida, e com `_GLOBAL_UPDATE_PUBLISHER` preenchido em `define.h` só o publicador da Upp é aceito. Enquanto a Upp não tem certificado, a constante fica vazia e o instalador sem assinatura é aceito só pelo SHA-256. O instalador roda com `/SILENT /ATUALIZACAO=1`: o Windows pede a permissão de administrador, o instalador fecha o app e o abre de novo como o usuário. Durante uma chamada a instalação é recusada. Se a conferência falhar, nada é instalado e o app oferece abrir a página no navegador.

O instalador do Inno Setup começa sem elevação e pede a permissão ao Windows ele mesmo; por isso o app acompanha o processo dele: se terminar com erro com o app ainda aberto (código 2 = "Não" no UAC, medido na VM), o aviso explica que a atualização não foi instalada e volta a oferecer o "Sim".

As conferências foram testadas fora do app (`Provisioning/UpdateDownload.cpp` com um servidor local): instalador correto, arquivo adulterado, outra versão, SHA-256 ausente ou malformado, 404, executável assinado válido, assinado e adulterado, publicador exigido sem assinatura e assinado por outro. **O fluxo completo foi testado na VM** (19/10/2026): oferta da 1.4.1 pelo painel, download por HTTPS com certificado validado e redirecionamento 302, conferência, UAC recusado (aviso correto depois da correção) e aceito, instalação silenciosa, app reaberto sozinho como o usuário, ainda ativado, informando 1.4.1 à Upp com a configuração aplicada. Bloquear a credencial de gestão também não revoga automaticamente a senha SIP na central.

## Compilação e homologação

Use o ambiente e as dependências fixadas descritos no [README](../README.md). O projeto inclui as novas fontes C++ e continua exigindo Windows, MSVC/ATL/MFC, pjproject e as bibliotecas fixadas. Compile a revisão da branch `provisionamento` em Win32 e x64; um build da tag antiga não testa as alterações novas.

Na revisão de 15/10/2026, o build **Release Win32 e x64 passou** em cópia isolada com MSVC v145, Windows SDK 10.0.26100.0 e pjproject/bcg729/SQLiteCpp nos commits fixados. O executável não foi executado nessa validação e não houve chamada real. O backend passou em 67 testes SQLite e no ciclo de migração/reversão/reaplicação; essas evidências não substituem a homologação conjunta abaixo. Os testes de laboratório posteriores (19 e 20/10/2026), com o executável instalado, estão em [HOMOLOGACAO_LABORATORIO.md](HOMOLOGACAO_LABORATORIO.md).

Antes de distribuir uma nova release:

- Registrar commit, versão, arquitetura e SHA-256 do `.exe` e instalador efetivamente testados.
- Validar ativação, código expirado/revogado, retomada após falha, persistência após reinício e troca de usuário/computador.
- Validar aplicação de configuração durante/fora de chamada, confirmação de revisão, erro de registro, indisponibilidade do backend e reconexão.
- Testar áudio nos dois sentidos, headsets, DTMF, transferências e codecs negociados com a central, incluindo G.729.
- Perfil com TLS: a central precisa ter TLS (e SRTP, se usado) confirmado no cadastro, e o certificado dela precisa ser de uma CA confiável no Windows (pública ou distribuída pela TI) e conter o nome usado no perfil; certificado curinga é recusado (RFC 5922). O app só aceita TLS 1.2 ou mais novo e não cai para UDP: um certificado recusado aparece como "Certificado não confiável" ou "Certificado não confere", e uma central só com TLS 1.0/1.1 (ou sem cifra em comum) como "Falha no TLS da central". O SRTP usa chaves no SDP (SDES), enviadas só por TLS numa conta TLS; DTLS-SRTP não está disponível.
- Cobrir Windows, rede/NAT/VPN e formas de operação de filas existentes nos clientes; usar os casos T01–T20 e FQ01–FQ12 da especificação.
- Encerrar as pendências de licença e assinatura digital antes da distribuição comercial correspondente.

Nos testes de filas SyssVoIP, separar recebimento de chamadas de fila, login/pausa de agente, monitoramento pela central, indicadores no painel UPP e supervisão de áudio. SIP registrado, DND, BLF ou heartbeat não demonstram essas capacidades. O laudo deve registrar **suportado**, **não suportado** ou **não confirmado**, com evidências do PBX e limites por perfil de cliente.

Os documentos de especificação Markdown/Word preservam o levantamento histórico da base 1.3.1. As entregas ali previstas precisam de evidência própria de implementação e aceite; esta atualização de documentação não substitui os testes de telefonia nem os testes automatizados do backend.
