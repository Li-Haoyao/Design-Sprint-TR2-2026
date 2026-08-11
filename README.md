# CareMate AI — Direct GitHub Prototype

This version keeps the GitHub connection **internal to the front-end logic**. There is no visible GitHub settings panel on the CareMate interface.

## Final flow

### Health Check

```text
CareMate Health Check
→ Confirm Health Check
→ browser updates data/health_input.json through GitHub REST API
→ commit is created automatically
→ GitHub Actions starts automatically
→ C++ health_analysis.cpp
→ Gemini API
→ data/health_result.json
→ GitHub Pages redeploy
→ open CareMate page polls for the matching requestId
→ Dashboard / Alerts & Trends / Health Report update automatically
```

### AI Assistant

```text
Ask AI
→ browser updates data/chat_input.json through GitHub REST API
→ commit is created automatically
→ GitHub Actions starts automatically
→ C++ ai_chat.cpp
→ OpenRouter API
→ data/chat_result.json
→ GitHub Pages redeploy
→ CareMate page displays the matching answer automatically
```

## One-time repository setup

1. Upload this entire project to a GitHub repository on branch `main`.
2. In **Settings → Pages**, choose **GitHub Actions** as the source.
3. In **Settings → Secrets and variables → Actions**, create:
   - `GEMINI_API_KEY`
   - `OPENROUTER_API_KEY`
4. Optional Variables:
   - `GEMINI_MODEL`
   - `OPENROUTER_MODEL`
5. If workflow `git push` receives a 403, enable **Read and write permissions** under **Settings → Actions → General → Workflow permissions**.

## Browser GitHub permission

The webpage does **not** display a GitHub configuration panel.

On the first Health Check or AI Assistant submission in a browser session, the browser asks once for a GitHub fine-grained personal access token. Use a token that is restricted to this CareMate repository and has:

```text
Repository permission:
Contents → Read and write
```

The token is stored only in `sessionStorage` for the current browser session. It is not written into `index.html`, JSON files, GitHub commits, or GitHub Actions logs.

When the browser session ends, the token must be entered again.

## Automatic repository detection

When the website runs on:

```text
https://OWNER.github.io/REPOSITORY/
```

the JavaScript automatically detects:

```text
owner  = OWNER
repo   = REPOSITORY
branch = main
```

Therefore the CareMate page does not need a visible username/repository setup screen.

## Files

```text
CareMate_AI_FINAL_DIRECT_GITHUB/
├── index.html
├── README.md
├── .nojekyll
├── .gitignore
├── cpp/
│   ├── common.hpp
│   ├── health_analysis.cpp
│   └── ai_chat.cpp
├── data/
│   ├── patient.json
│   ├── history.json
│   ├── health_input.json
│   ├── health_result.json
│   ├── chat_input.json
│   └── chat_result.json
└── .github/
    └── workflows/
        ├── deploy-pages.yml
        ├── health-analysis.yml
        └── ai-chat.yml
```

## Important

This is a university Design Sprint prototype using synthetic Mary Tan data. Do not use real private health information or treat the AI output as medical diagnosis.


## Automatic progress + page refresh

After a Health Check or AI Assistant submission, the page now tracks the matching GitHub Actions run internally and shows only user-friendly progress messages such as:

```text
Submitted
Queued
Analysing
Updating
Complete
```

Technical GitHub/API details are not shown in the CareMate interface.

When the matching `requestId` appears in the newly published result file:

- Health Check automatically refreshes the page once and returns to **Dashboard**.
- AI Assistant automatically refreshes the page once and returns to **AI Assistant**, where the latest answer is shown.

A session flag prevents refresh loops.


## Fixed progress monitoring

This build does **not** query the GitHub Actions workflow-runs API from the browser.

The previous build could remain on `Waiting for analysis to start...` when a fine-grained browser token had only `Contents: Read and write`, because reading workflow runs is a separate GitHub Actions permission.

This fixed build keeps the browser token requirement minimal:

```text
Contents → Read and write
```

After submission it checks the generated repository result (`health_result.json` or `chat_result.json`) through the Contents API. The user still sees simple progress messages:

```text
Submitted
Analysing
Processing
Analysis complete
```

The normal GitHub Pages polling continues in parallel. When the published result has the matching `requestId`, the page refreshes automatically.


## Important update — automatic start

This version no longer relies on a `push` event to happen to start the AI workflow.

After the page writes the input JSON, it directly calls GitHub's workflow-dispatch API and starts the correct workflow automatically.

The browser fine-grained token therefore needs these repository permissions:

```text
Contents → Read and write
Actions  → Read and write
```

The CareMate page still does not show GitHub controls to the end user. The token remains in browser `sessionStorage` for the current session only.

If the C++ build or external AI API fails, the workflow now creates a matching fallback result so the webpage does not remain permanently on a processing message.
