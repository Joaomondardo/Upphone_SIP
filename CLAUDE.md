# Upphone SIP

Softphone Windows C++/MFC baseado no MicroSIP/pjproject. Responda em português. Entradas em `docs/MAPA_MODULOS.md`. Decisões históricas em `docs/CONTEXTO_ARQUIVADO.md` e `docs/HISTORICO.md`: consulte somente o assunto pertinente, confirmando fontes atuais.

- Use linguagem clara para estudantes: explique termos na primeira ocorrência e inclua exemplos quando ajudarem. Comece por `docs/GUIA_LEITURA.md` para escolher o assunto. Nos comentários novos, explique por que o código precisa daquela regra. Preserve nomes e comportamento dos contratos existentes.

- Preserve alterações em andamento. Build exige este repo dentro de `<pjproject>/microsip`: `build/setup.ps1` e `build/build.ps1`. Dependências fixadas em `build/pins.ps1`; só atualize a pedido.
- Use cores `_GLOBAL_THEME_*`/`_GLOBAL_BRAND_ACCENT` e comentários `// Upphone: ...` para alterações em relação ao MicroSIP.
- Confira a ordem de aplicação dos valores em `settings.cpp` e `AccountDlg.cpp`: conta manual usa INI; conta configurada pelo servidor usa `provisioning.dat`. Grave e recarregue sem interromper chamadas. DPAPI, a proteção de dados do Windows, depende do usuário e da máquina que recebem os dados. Veja `docs/ORDEM_CONFIGURACOES.md`.
- Não versionar dados locais, logs, senhas, tokens ou certificados. TLS/Schannel e SRTP exigem `build/pjproject/config_site.h` e o patch de RNG seguro aplicado pelo build.
- Testes de interface: somente janela do app via PrintWindow ou comandos documentados. Não capturar tela inteira nem enviar mensagens à janela em primeiro plano.
- Build/laboratório não comprovam homologação na central real. Revogar API não revoga senha SIP no PBX. Para provisionamento/telefonia: `docs/PROVISIONAMENTO.md` e `docs/HOMOLOGACAO_LABORATORIO.md`. Para distribuição/licenças: especificação e avisos atuais dos fontes. Mantenha DOCX/Markdown correspondentes ao revisar a especificação.
- Graphify: `graphify query "simbolo" --budget 1200`. Confirme os fontes, especialmente macros MFC; use `rg` se inconclusivo. Após concluir mudanças de código, rode `graphify update .` uma vez. Não ler grafo/relatório inteiro nem gerar análise semântica com LLM automaticamente.
