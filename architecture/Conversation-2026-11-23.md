# Conversation 2026-11-23

- User: "status ?" — 1.3.0 released; GitHub main at 9538d6d, local fb26516 not yet pushed (docs only).
- User: "fyi: we should default to the cheapest AI option in the Assistant."
- Checked the API docs: Haiku 5.5 answers a request with "fallbacks" with 400, and we send fallbacks "default" on every request, so in 1.3.0 choosing Haiku made every question fail (the user's live test used the Opus default). Haiku 5.5 also has a second rate card: prompt over 100K tokens at $0.50 / $2.50 (5 ×).
- Asked: model only or also effort Low; a 1.3.1 now? User: "yes, keep medium, and start on the next items we will bundle all in the next version".
- 22f done: defaultModel claude-haiku-5-5 (Claude.h; PluginEditor and the settings dialog use it); hasFallback(model): fallbacks and the beta header only for Opus and Sonnet; Price.longAbove/longFactor (Haiku 100K, 5 ×; kept by a price_ line); Spec S12.3/S12.6/S12.7, Lesson 12 (Haiku default, "well under a cent"), Changes (Unreleased), T-S12-haiku; native 34 of 34.
- L10 done: the default rack rows put LFO 1 before VCO 1 and ENV 2 → VCF before ENV 1 → VCA (reordered by title when the rack is built; panel numbers unchanged, so saved layouts load as before; the layout is stored only after the user moves a panel, so users who never did get the new order). Checked on Xvfb with an empty HOME (T-S6.5-default); Spec S6.5, Changes, Plan, Tests.
