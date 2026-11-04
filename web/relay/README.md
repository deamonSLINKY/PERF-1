# The library relay

The Pieces page submits pieces to the Library through this small Cloudflare Worker and reads the Library back from it. A submission is checked (it must decode as a real PERF-1 piece or set; its title, name and note short and plain) and is in the Library at once. Nobody needs an account, and no GitHub issue is involved.

It runs on Cloudflare's free plan: 100,000 requests and 1,000 KV writes a day, far more than the Library needs. Reading the list is one KV read, cached for a minute.

## Set it up (once, about 10 minutes)

1. Make a free account at https://dash.cloudflare.com/sign-up.
2. In a terminal with Node (the repo's `tc/node-v22.11.0-*` works), from `web/relay/`:
   ```
   npx wrangler login
   npx wrangler kv namespace create LIB
   ```
   Put the `id` it prints into `wrangler.toml` (`PUT-THE-ID-HERE`).
3. Choose a long random admin secret (a password manager can make one) and store it:
   ```
   npx wrangler secret put ADMIN
   ```
   Keep it: it removes any entry, and it signs the keys submitters get to remove their own.
4. Deploy:
   ```
   npx wrangler deploy
   ```
   It prints the address, like `https://perf1-library.YOURNAME.workers.dev`.
5. Put that address in `web/relay/relay.json` as `{"url": "https://perf1-library.YOURNAME.workers.dev"}` and rebuild the site. Until then, the page's Submit button opens a pre-filled GitHub issue as before.

## Removing an entry

- On the site: open `pieces/#admin=YOUR-SECRET` once in your browser. The page keeps the secret in that browser only, and every Library card gets a **Remove** button. `#admin=` with nothing after it forgets it.
- Or from a terminal: `curl -X POST https://…workers.dev/remove -H "Content-Type: application/json" -d '{"id":"ID","del":"YOUR-SECRET"}'`.

A submitter can remove what they submitted from the same browser: the page keeps a key for each of their submissions.

## Limits

- 16 codes a submission, 30 an hour from one address.
- The same code submitted twice is one entry.
- The curated list in `web/library/library.json` stays: it is shown first, marked as picked.
