# SEO page — gravador de áudio para podcast — implementation plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `gravador-podcast.html` — an SEO-optimized product page targeting "gravador de áudio para podcast" keyword cluster.

**Architecture:** Single static HTML file reusing the design system from `index.html` (Tailwind CDN, same fonts/colors/component styles). Includes JSON-LD structured data for SoftwareApplication and FAQPage schemas.

**Tech Stack:** HTML, Tailwind CSS (CDN), vanilla JS (scroll animations only)

**Spec:** `docs/superpowers/specs/2026-03-15-seo-page-design.md`

---

### Task 1: Create gravador-podcast.html

**Files:**
- Create: `gravador-podcast.html`

- [ ] **Step 1: Create the full page**

Create `gravador-podcast.html` with the following structure. Use **exactly** the same `<head>` setup as `index.html` (Tailwind config, fonts, CSS) but with SEO-specific meta tags:

**Head:**
- `<title>Gravador de áudio para podcast grátis — BDG rec</title>`
- Meta description: "Grave seu podcast com áudio profissional. Normalização, redução de ruído e compressor integrados. Grátis para macOS e Windows. Sem cadastro."
- Canonical: `https://rec.bdg.fm/gravador-podcast`
- Open Graph tags (og:title, og:description, og:image, og:url, og:type)
- JSON-LD: SoftwareApplication schema
- JSON-LD: FAQPage schema
- Same Tailwind config, fonts, and CSS from index.html
- `lang="pt-BR"` (no language switcher)

**Nav:**
- Same sticky nav as index.html
- Logo links to `/` (home)
- No language switcher — just the "Baixar grátis" button linking to `#download`

**Hero section:**
- H1: "Gravador de áudio para podcast"
- Badge: "Aplicativo de gravação de áudio"
- Subtitle: "O aplicativo de gravação de áudio para podcast mais simples que existe. Um botão, áudio profissional, tratamento integrado. Grátis para macOS e Windows."
- Download buttons (same URLs as index.html)
- App screenshot (same `layout-bdg-rec.png`)

**Problem/solution section:**
- H2: "Gravar áudio de podcast não precisa ser complicado"
- Text explaining that typical setups require DAWs, plugins, configuration
- Position BDG rec as the simple alternative: "Com o BDG rec, você aperta um botão e o seu áudio sai pronto."

**Features grid (6 cards):**
Each card reuses the `feature-card` class from index.html. H3 headings use keyword variations:
1. "Gravação de áudio profissional em WAV 24-bit" — mono recording, native sample rate
2. "Redução de ruído automática para podcast" — RNNoise neural network
3. "Compressor de áudio integrado para voz" — 3-band, auto makeup gain
4. "Gravador de podcast à prova de falhas" — 5-min chunk rotation, crash recovery
5. "Normalização automática de áudio" — RMS normalization, brick-wall limiter
6. "De-esser para voz mais limpa" — 4-8 kHz sibilance reduction

**How it works (3 steps):**
Same `step-num` component from index.html:
1. "Selecione o microfone" — "Escolha seu dispositivo de áudio. O gravador de podcast salva sua preferência."
2. "Aperte REC" — "Um botão grande no centro. Seu áudio é capturado em WAV 24-bit."
3. "Áudio profissional pronto" — "O app aplica normalização, redução de ruído e compressão automaticamente."

**FAQ section (accordion):**
HTML `<details>/<summary>` elements (no JS needed). 6 questions:
1. "Qual o melhor aplicativo para gravar áudio de podcast?" → BDG rec, explain why
2. "Como gravar áudio de podcast no Mac?" → Download DMG, install, press REC
3. "Preciso de um DAW para gravar podcast?" → No, BDG rec has everything built-in
4. "O BDG rec funciona no Windows?" → Yes, Windows 10+ x64
5. "O aplicativo é realmente grátis?" → Yes, free and open source
6. "Qual formato de áudio é melhor para podcast?" → WAV 24-bit, explain why

Style `<details>` with:
```css
details { background: #19191b; border: 1px solid rgba(255,255,255,0.06); border-radius: 12px; padding: 0; margin-bottom: 12px; }
details summary { padding: 20px 24px; cursor: pointer; font-weight: 600; font-size: 15px; list-style: none; }
details summary::-webkit-details-marker { display: none; }
details[open] summary { border-bottom: 1px solid rgba(255,255,255,0.06); }
details .faq-answer { padding: 20px 24px; color: rgba(255,255,255,0.5); font-size: 14px; line-height: 1.7; }
```

**CTA section:**
Same as index.html `#download` section: REC pulse button, "Pronto para gravar?", download buttons, compatibility line.

**Footer:**
Same as index.html but add link to home page.

**Script:**
Only the scroll fade-in observer (no language switcher).

**JSON-LD (in `<head>`):**

SoftwareApplication:
```json
{
  "@context": "https://schema.org",
  "@type": "SoftwareApplication",
  "name": "BDG rec",
  "description": "Gravador de áudio para podcast com normalização, redução de ruído e compressor integrados.",
  "applicationCategory": "MultimediaApplication",
  "operatingSystem": "macOS, Windows",
  "offers": { "@type": "Offer", "price": "0", "priceCurrency": "BRL" },
  "downloadUrl": "https://rec.bdg.fm/gravador-podcast",
  "softwareVersion": "1.1.0"
}
```

FAQPage:
```json
{
  "@context": "https://schema.org",
  "@type": "FAQPage",
  "mainEntity": [
    { "@type": "Question", "name": "Qual o melhor aplicativo para gravar áudio de podcast?", "acceptedAnswer": { "@type": "Answer", "text": "..." } },
    ...
  ]
}
```

- [ ] **Step 2: Commit**

```bash
git add gravador-podcast.html
git commit -m "feat: add SEO page for gravador de áudio para podcast"
```

---

### Task 2: Update deploy.sh and index.html interlink

**Files:**
- Modify: `deploy.sh`
- Modify: `index.html`

- [ ] **Step 1: Add gravador-podcast.html to deploy.sh FILES array**

Add `"gravador-podcast.html"` after `"admin.html"` in the FILES array.

- [ ] **Step 2: Add internal link from index.html footer**

In `index.html` footer, add a link to the SEO page. After the "Gratuito e open source" text, add:
```html
<a href="/gravador-podcast" class="text-white/20 hover:text-white/40 transition-colors text-xs">Gravador de podcast</a>
```

- [ ] **Step 3: Commit**

```bash
git add deploy.sh index.html
git commit -m "feat: add SEO page to deploy and interlink from footer"
```

---

### Task 3: Deploy and verify

- [ ] **Step 1: Deploy**

```bash
bash deploy.sh
```

- [ ] **Step 2: Create .htaccess for extensionless URLs**

Upload `.htaccess` to SiteGround to allow `/gravador-podcast` without `.html`:

```apache
RewriteEngine On
RewriteCond %{REQUEST_FILENAME} !-f
RewriteCond %{REQUEST_FILENAME} !-d
RewriteCond %{REQUEST_FILENAME}.html -f
RewriteRule ^(.+)$ $1.html [L]
```

- [ ] **Step 3: Verify page loads at both URLs**

```bash
curl -sI https://rec.bdg.fm/gravador-podcast.html | head -5
curl -sI https://rec.bdg.fm/gravador-podcast | head -5
```

- [ ] **Step 4: Validate structured data**

Check JSON-LD in page source for valid SoftwareApplication and FAQPage schemas.
