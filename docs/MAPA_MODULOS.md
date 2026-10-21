# Mapa dos módulos — aplicativo

Este mapa mostra onde procurar cada assunto. Conferido nos arquivos em 20/10/2026; confirme o estado atual no Git e no código. Na tabela, `Nome.cpp/.h` significa os arquivos `Nome.cpp` e `Nome.h`. Introdução e termos: [guia de leitura](GUIA_LEITURA.md).

| Área | Arquivos para começar | O que esta parte faz |
| --- | --- | --- |
| Inicialização/Windows | `microsip.cpp/.h`, `global.cpp/.h` | Ciclo do app e funções compartilhadas |
| Janela principal/SIP | `mainDlg.cpp/.h`, `Dialer.cpp/.h` | Coordena telas, telefonia e funções chamadas quando um evento acontece |
| Conta/configuração | `settings.cpp/.h`, `AccountDlg.cpp/.h`, `MyAccountDlg.cpp/.h` | Persistência, conta manual/provisionada, parâmetros SIP |
| Provisionamento | `Provisioning/Provisioning.cpp/.h`, `ProvisioningHttp.cpp/.h`, `ProvisioningSupport.cpp/.h` (na mesma pasta) | Estado remoto, HTTP e aplicação de configuração |
| Ativação/atualização | `ActivationDlg.cpp/.h`, `Provisioning/UpdateDownload.cpp/.h`, `UpdateDlg.cpp/.h` | Ativação e validação de atualizações |
| Contatos | `Contacts.cpp/.h`, `Service/ContactsService.cpp/.h`, `Data/ContactsRepository.cpp/.h` | Tela, importação/exportação, regras e armazenamento |
| Histórico de chamadas | `Calls.cpp/.h`, `Service/CallsService.cpp/.h`, `Data/CallsRepository.cpp/.h` | Histórico, consultas e importação/exportação |
| Modelo/banco | `Domain/`, `Data/Database.cpp/.h`, `Data/I*Repository.h` | Entidades, contratos e SQLite |
| XML/conversões | `Utils/Xml.cpp/.h`, `Utils/StringConverter.h` | Lê a estrutura XML, converte caracteres especiais e transforma textos |
| Áudio/chamadas | `AudioDlg.cpp/.h`, `RinginDlg.cpp/.h`, `Transfer.cpp/.h`, `MMNotificationClient.h` | Dispositivos, recebimento, transferência e notificações |
| Aparência | `Theme.cpp/.h`, `ThemeDialog.cpp/.h`, `BaseDialog.cpp/.h`, `define.h`, `Button*`, `DarkTabCtrl*` | Temas e controles MFC |
| Recursos/idioma | `main.rc`, `resource.h`, `res/`, `lib/langpack.cpp/.h` | Diálogos, ícones, sons e traduções |
| Build/distribuição | `microsip.vcxproj`, `build/build.ps1`, `build/setup.ps1`, `build/pins.ps1`, `build/Upphone.iss`, `const.h` | MSBuild, dependências, instalador e versão |
| Auxiliares | `lib/` | Funções de apoio e bibliotecas; confira avisos de licença e inclusão no arquivo de compilação vcxproj |

Fluxos para orientar busca (confirme cada operação no fonte): contatos → serviço/repositório → XML/CSV/banco; backend → Provisioning → settings/conta → telefonia; tela/serviço → interface de repositório → implementação → Database.

Na pasta do aplicativo: `graphify query "ContactsService Xml" --budget 1200`. Se a consulta não ajudar, procure no texto: `rg -n "simbolo" <arquivo-ou-pasta>`. Leia os trechos relevantes de mainDlg.cpp. O número mostrado pelo rg é a linha do arquivo.

Build conforme README. No ambiente pjproject já preparado/compatível: `./build/build.ps1 -Platforms x64 -SkipPjproject`. Mudanças em pjproject exigem recompilação correspondente; build não substitui teste de voz/central.

Documentação por assunto: `PROVISIONAMENTO.md` (contratos), `HOMOLOGACAO_LABORATORIO.md` (evidências), `ESPECIFICACAO_SOFTPHONE_UPP.md` (escopo/distribuição), `CONTEXTO_ARQUIVADO.md` e `HISTORICO.md` (decisões históricas). Não carregar tudo por padrão.
