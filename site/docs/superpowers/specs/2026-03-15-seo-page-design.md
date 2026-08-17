# SEO page — gravador de áudio para podcast

## Overview

New page at `rec.bdg.fm/gravador-podcast.html` targeting the keyword cluster around "aplicativo de gravação de áudio para podcast". Product-focused page (not editorial) with same design system as index.html.

## URL and metadata

- **File:** `gravador-podcast.html`
- **URL:** `https://rec.bdg.fm/gravador-podcast` (SiteGround can handle extensionless via .htaccess)
- **Title tag:** `Gravador de áudio para podcast grátis — BDG rec`
- **Meta description:** `Grave seu podcast com áudio profissional. Normalização, redução de ruído e compressor integrados. Grátis para macOS e Windows. Sem cadastro.`
- **Canonical:** `https://rec.bdg.fm/gravador-podcast`
- **Language:** pt-BR only (no language switcher — SEO page targets Brazilian Portuguese)

## Target keywords

Primary: "gravador de áudio para podcast"

Variations used organically throughout content:
- aplicativo de gravação de áudio para podcast
- app para gravar podcast
- gravador de podcast grátis
- software de gravação de áudio para podcast
- gravar podcast no mac
- gravador de áudio profissional

All keywords focus on AUDIO to avoid confusion with video recording tools.

## Page structure

### 1. Nav
Same nav as index.html but links back to home. No language switcher.

### 2. Hero
- H1: "Gravador de áudio para podcast"
- Subtitle: value proposition emphasizing simplicity + professional audio + free
- Two download buttons (macOS DMG, Windows EXE) — same links as index.html
- App screenshot

### 3. Problem/solution section
- H2: "Gravar áudio de podcast não precisa ser complicado"
- Short text about complexity of typical podcast setups (DAWs, plugins, config)
- Position BDG rec as the simple alternative

### 4. Features (keyword-rich)
Grid of 6 cards, each with a keyword variation as heading:
- "Gravação de áudio profissional em WAV 24-bit"
- "Redução de ruído automática para podcast"
- "Compressor de áudio integrado para voz"
- "Gravador de podcast à prova de falhas"
- "Normalização automática de áudio"
- "De-esser para voz mais limpa"

Each card: icon + H3 heading + 2-line description. Same `feature-card` styling as index.html.

### 5. How it works
3 steps reused from index.html but with SEO-optimized text:
1. Selecione o microfone
2. Aperte REC
3. Áudio profissional pronto

### 6. FAQ (with Schema.org markup)
5-6 questions in accordion format, with `FAQPage` structured data (JSON-LD):
- "Qual o melhor aplicativo para gravar áudio de podcast?"
- "Como gravar áudio de podcast no Mac?"
- "Preciso de um DAW para gravar podcast?"
- "O BDG rec funciona no Windows?"
- "O aplicativo é realmente grátis?"
- "Qual formato de áudio é melhor para podcast?"

### 7. CTA final
Same as index.html CTA section: REC button, "Pronto para gravar?", download buttons, compatibility info.

### 8. Footer
Same as index.html.

## Structured data (JSON-LD)

### SoftwareApplication schema
```json
{
  "@context": "https://schema.org",
  "@type": "SoftwareApplication",
  "name": "BDG rec",
  "description": "Gravador de áudio para podcast com normalização, redução de ruído e compressor integrados.",
  "applicationCategory": "MultimediaApplication",
  "operatingSystem": "macOS, Windows",
  "offers": {
    "@type": "Offer",
    "price": "0",
    "priceCurrency": "BRL"
  },
  "downloadUrl": "https://rec.bdg.fm/gravador-podcast",
  "softwareVersion": "1.1.0"
}
```

### FAQPage schema
Each FAQ question/answer pair in structured data for rich snippets in Google.

## Open Graph tags
```html
<meta property="og:title" content="Gravador de áudio para podcast grátis — BDG rec">
<meta property="og:description" content="Grave seu podcast com áudio profissional. Grátis para macOS e Windows.">
<meta property="og:image" content="https://rec.bdg.fm/layout-bdg-rec.png">
<meta property="og:url" content="https://rec.bdg.fm/gravador-podcast">
<meta property="og:type" content="website">
```

## Internal linking
- index.html links to gravador-podcast.html (footer or nav)
- gravador-podcast.html links back to index.html (nav logo)
- Both pages link to the same GitHub releases for downloads

## Design system
Same as index.html:
- Dark theme (#0e0e10 background)
- Tailwind CSS via CDN
- Fonts: Instrument Sans (body) + Space Mono (display)
- BDG primary: #E5067D
- Same component styles (feature-card, dl-btn, fade-up animations)
- Responsive, mobile-first

## Deploy
Add `gravador-podcast.html` to deploy.sh FILES array.

## What is NOT in scope
- Blog/editorial content
- Multi-language (PT-BR only)
- CMS or dynamic content
- Analytics tracking on this page (uses same Plausible/GA if added later)
