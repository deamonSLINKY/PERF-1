# The OPEN-1 page

Everything on https://rustypawz.github.io/fm1-HYP1/open1/ comes from this folder. The site rebuilds on every push to `main`, which takes a few minutes, because the firmware builds first.

## Post an update

1. On GitHub, open `open1/updates/` and choose **Add file › Create new file**.
2. Name it `YYYY-MM-DD-short-name.md`, for example `2026-11-02-breadboard-boots.md`. The short name becomes the update's link (`open1/#breadboard-boots`).
3. Start it like this, then write the update below in Markdown:

   ```
   ---
   title: The breadboard boots
   date: 2026-11-02
   tag: Prototype
   ---
   HYP-1 is running on the Daisy Seed. Here's the first sound out of it...
   ```

4. Commit to `main`. The newest update goes to the top of the timeline and into the RSS feed (`open1/feed.xml`).

The `tag` is a free label shown as a chip, such as Milestone, Design, Prototype, Firmware or Funding. To change an update, edit its file; to remove one, delete it.

**Pictures:** upload them to `open1/media/`, then use `![what it shows](media/photo.jpg)` in the update. Keep photos under about 1 MB each.

## Settings (`site.json`)

| Setting | What it does |
| --- | --- |
| `phase` | Where the project is: 0 Idea and pitch, 1 Breadboard prototype, 2 First boards, 3 Emulator, SDK and firmware jam, 4 Crowdfunding, 5 Manufacturing and shipping. Earlier phases show as Done. |
| `youtube` | The pitch video's YouTube id (the part after `watch?v=`). Empty: the page plays `media/open1-pitch.mp4` itself. |
| `buttondown` | Your Buttondown username. Empty: the sign-up form opens the visitor's email app with a message to `email`. |
| `email`, `kofi` | The contact address and Ko-fi link on the page. |

## The mailing list

Until `buttondown` is set, sign-ups arrive as emails to itsme@redtail.works. To collect them properly:

1. Make a free account at buttondown.com and note your username.
2. Put it in `site.json` as `"buttondown": "yourname"`. The form then sends sign-ups straight to your Buttondown list. The list is set to `open1`.
3. Optional: in Buttondown, connect the RSS feed (`https://rustypawz.github.io/fm1-HYP1/open1/feed.xml`) so every update you post can also go out as an email.

## Goals, specs and prices

These are in `tools/site/open1.py`, near the top: `PHASES`, `GOALS`, `ENGINE`, `INSTRUMENT` and `PRICES`.
