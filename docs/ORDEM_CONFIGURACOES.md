# Qual configuração prevalece?

Há duas perguntas diferentes: como o servidor escolhe os valores e como o aplicativo os aplica. Uma alteração em um arquivo INI pode ser sobrescrita depois pela configuração remota ou por um valor fixado no código.

## No servidor: perfil, ramal e ambiente

Em `upp-provisionamento/app/services/provisioning.py`, `effective_values` começa com o perfil SIP, aplica as alterações do ramal e, por último, aplica os campos definidos pela política do ambiente.

| Camada | Função | Prioridade |
| --- | --- | --- |
| Perfil SIP | Valores compartilhados por vários ramais | Menor |
| Alterações do ramal (`overrides`) | Exceções para um ramal específico | Intermediária |
| Política do ambiente | Valores impostos para os campos que ela define | Maior |

Exemplo para o campo `transport`: perfil = `udp`; ramal = `tcp`; política = `tls`. O resultado é `tls`. Se a política não definir esse campo, o resultado será `tcp`. A prioridade vale para os campos listados em `PROFILE_FIELDS`, não para todo atributo do sistema.

`build_config` transforma esse resultado no documento enviado ao aplicativo. Nome de usuário e senha SIP vêm do cadastro do ramal; eles não seguem automaticamente a mesma regra de todos os campos do perfil. `snapshot` guarda uma descrição sem a senha para o histórico e informa a origem de cada campo.

## No aplicativo: INI, valores fixos e estado remoto

`AccountSettings::Init`, em `settings.cpp`, carrega as configurações e contas locais e chama `provisioning.Init()` ao final. Esse último passo lê o estado remoto guardado no computador.

Em `SettingsLoad`, o código lê valores locais, chama `ResetProvisionedFields` e depois `ApplySettingsOverrides`. Os campos reiniciados são codecs, método DTMF, STUN, código de transferência e log. DTMF é a forma de enviar os dígitos pressionados durante a chamada; STUN ajuda o aplicativo a descobrir informações de rede.

| Situação | Resultado |
| --- | --- |
| Não há conta remota válida | Não se aplicam as substituições do servidor; permanecem as regras locais e os padrões definidos pelo código |
| Há conta remota válida | `ApplySettingsOverrides` substitui os campos suportados e `ApplyAccount` usa a conta recebida do servidor |
| Edição de conta manual | `AccountDlg.cpp` aplica suas próprias regras ao salvar; na versão examinada, força transporte UDP |
| Configuração inválida ou de outro ambiente | `ApplyConfig` recusa a aplicação; não trate o recebimento como sucesso |
| Mudança reiniciaria conta/áudio e há chamada | A configuração fica pendente até terminar a chamada |
| Falha ao gravar o estado protegido | O aplicativo mantém o estado anterior em vez de confirmar a nova revisão como aplicada |

Conta manual usa o INI. O estado da conta entregue pelo servidor fica em `provisioning.dat`, protegido pela DPAPI do Windows. Esse arquivo também pode guardar uma configuração pendente; não é um INI para editar manualmente.

## Ao investigar um valor inesperado

Escolha um campo e siga: cadastro/perfil/política → `effective_values` → `build_config` → `ParseConfig` → `ApplyConfig` → `ApplySettingsOverrides` ou `ApplyAccount`. Compare a revisão desejada no servidor com o estado que o aplicativo informou: aplicado, pendente ou recusado. Depois confirme o valor após reiniciar o aplicativo e, quando pertinente, na central telefônica.
