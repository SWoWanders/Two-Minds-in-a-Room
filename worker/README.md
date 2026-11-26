# Smart mode: free cloud minds for Pip and Moss

Smart mode lets the robots think with much bigger models than a phone can run: **Llama 3.1 8B** for Pip and **Qwen3 30B** for Moss. These run on Cloudflare Workers AI, using your own free Worker.

## It stays free

- Deploy this on the **Workers Free plan**, and never add a payment method or upgrade to Workers Paid for this.
- Workers AI gives every account **10,000 Neurons a day** at no charge. One robot turn uses about 10 to 15 Neurons, so that's roughly 700 turns a day.
- On the Free plan, going over the daily allowance does **not** bill you. Requests just fail until it resets at 00:00 UTC. The app notices this and switches the robots back to their on-phone minds by itself.
- Optional extra guard: set your own lower daily cap (see the end of this page).

## Set it up (about 5 minutes, no coding)

1. Sign in at dash.cloudflare.com. A free account is fine.
2. Go to **Workers & Pages**, choose **Create**, then **Create Worker**. Name it `two-minds-smart` and choose **Deploy**.
3. Choose **Edit code**. Delete everything in the editor, paste in the whole of `smart-mode.js` from this folder, then choose **Deploy**.
4. Go back to the Worker's page, open **Settings**, then **Bindings**. Choose **Add binding**, pick **Workers AI**, set the variable name to `AI`, and save.
5. Copy the Worker's address. It looks like `https://two-minds-smart.YOURNAME.workers.dev`.
6. In Pip and Moss, open **Smart mode** (in the settings under the chat), paste the address, set it to **Smart mode on**, and tap **Test**. It should say "Connected".

That's it. Tap **Start with AI** and the robots will think in the cloud whenever it answers.

## Who can use your Worker

The Worker only answers requests from the Pip and Moss web page (`https://swowanders.github.io`) and the Android app. If you host the page somewhere else, add that address to `ALLOWED_ORIGINS` at the top of `smart-mode.js`.

Other people could still call it from outside a browser. The worst they could do is use up your free daily allowance; it can't cost money on the Free plan.

## Optional: your own daily cap

1. In the dashboard, go to **Storage & Databases**, then **KV**, and create a namespace called `two-minds-usage`.
2. In the Worker's **Settings**, then **Bindings**, add a **KV namespace** binding with the variable name `USAGE`, pointing to that namespace.
3. In **Settings**, then **Variables and Secrets**, add a text variable `DAILY_LIMIT` set to the most turns you want per day (for example `400`).

When the cap is reached, the robots switch to their on-phone minds until the next UTC day. Each turn uses one KV write, and KV's free tier allows 1,000 a day, so keep `DAILY_LIMIT` under 1,000.

## Using the command line instead

If you use Wrangler: `npx wrangler deploy` from this folder deploys it with the AI binding already set in `wrangler.toml`.
