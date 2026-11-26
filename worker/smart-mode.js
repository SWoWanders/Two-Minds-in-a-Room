// Two Minds, One Room — free "smart mode" for Pip and Moss.
// A tiny Cloudflare Worker that passes a robot's prompt to Workers AI and returns the reply.
//
// FREE-ONLY: deploy this on the Workers FREE plan. Workers AI includes 10,000 Neurons per day at no charge.
// On the Free plan, going over that does not bill you: requests simply fail until the daily reset (00:00 UTC),
// and the app falls back to the robots' on-phone minds automatically. One robot turn uses about 10 to 15 Neurons.
//
// Optional extra guard: bind a KV namespace as USAGE and set DAILY_LIMIT to cap turns per day yourself.

const MODELS = {
  // Pip: quick and lively. Moss: a different family, so the two robots still have different minds.
  pip: { id: '@cf/meta/llama-3.1-8b-instruct-fp8-fast', name: 'Llama 3.1 8B' },
  moss: { id: '@cf/qwen/qwen3-30b-a3b-fp8', name: 'Qwen3 30B', extra: { chat_template_kwargs: { enable_thinking: false } } }
};
const ALLOWED_ORIGINS = ['https://swowanders.github.io', 'https://localhost', 'http://localhost', 'capacitor://localhost'];

const cors = origin => ({
  'Access-Control-Allow-Origin': origin,
  'Access-Control-Allow-Methods': 'POST, OPTIONS',
  'Access-Control-Allow-Headers': 'content-type',
  'Access-Control-Max-Age': '86400',
  'Vary': 'Origin'
});
const reply = (body, status, origin) => new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json', ...cors(origin) } });

export default {
  async fetch(request, env) {
    const origin = request.headers.get('Origin') || '';
    const okOrigin = ALLOWED_ORIGINS.includes(origin) || /^http:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/.test(origin);
    if (!okOrigin) return new Response('Not allowed', { status: 403 });
    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: cors(origin) });
    if (request.method !== 'POST') return reply({ error: 'POST only' }, 405, origin);

    let body;
    try { body = await request.json(); } catch { return reply({ error: 'bad JSON' }, 400, origin); }
    if (body && body.ping) return reply({ ok: true, models: Object.fromEntries(Object.entries(MODELS).map(([k, m]) => [k, m.name])) }, 200, origin);

    const who = body && MODELS[body.who] ? body.who : 'pip';
    const msgs = Array.isArray(body && body.messages) ? body.messages.slice(-8) : [];
    const messages = msgs.filter(m => m && ['system', 'user', 'assistant'].includes(m.role) && typeof m.content === 'string')
      .map(m => ({ role: m.role, content: m.content.slice(0, 16000) }));
    const total = messages.reduce((a, m) => a + m.content.length, 0);
    if (!messages.length || total > 24000) return reply({ error: 'prompt missing or too long' }, 400, origin);
    const max_tokens = Math.max(16, Math.min(300, +body.max_tokens || 200));
    const temperature = Math.max(0, Math.min(1.5, +body.temperature || 0.8));

    // optional daily cap you control (needs a KV namespace bound as USAGE)
    if (env.USAGE && env.DAILY_LIMIT) {
      const key = 'turns-' + new Date().toISOString().slice(0, 10);
      const used = +(await env.USAGE.get(key)) || 0;
      if (used >= +env.DAILY_LIMIT) return reply({ error: 'daily limit reached', quota: true }, 429, origin);
      await env.USAGE.put(key, String(used + 1), { expirationTtl: 172800 });
    }

    const m = MODELS[who];
    try {
      const r = await env.AI.run(m.id, { messages, max_tokens, temperature, ...(m.extra || {}) });
      let text = typeof r.response === 'string' ? r.response : (r.choices && r.choices[0] && r.choices[0].message && r.choices[0].message.content) || '';
      text = String(text).replace(/<think>[\s\S]*?<\/think>/g, '').trim();
      return reply({ text, model: m.name, usage: r.usage || null }, 200, origin);
    } catch (e) {
      const msg = String((e && e.message) || e);
      const quota = /4006|neuron|quota|limit|daily|exceed/i.test(msg);
      return reply({ error: msg.slice(0, 200), quota }, quota ? 429 : 502, origin);
    }
  }
};
