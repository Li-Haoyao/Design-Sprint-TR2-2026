# CareMate AI — Final Design Sprint Prototype

This package uses the **latest Mary Tan front end**, a **C++ file-based workflow**, **Gemini API for Health Check analysis**, and **OpenRouter API for the separate AI Assistant**.

There is:

- no login or registration;
- no database;
- no always-on C++ web server;
- no Docker / Railway / Render requirement.

## Final architecture

```text
HEALTH CHECK
CareMate page
  ↓
health_input.json
  ↓ (upload/replace + commit)
GitHub Actions automatically starts
  ↓
C++: cpp/health_analysis.cpp
  ↓
Gemini API
  ↓
health_result.json + history.json
  ↓
GitHub Pages deploy
  ↓
open page polls every 5 seconds and updates
Dashboard / Alerts & Trends / Health Report


AI ASSISTANT
CareMate AI question
  ↓
chat_input.json
  ↓ (upload/replace + commit)
GitHub Actions automatically starts
  ↓
C++: cpp/ai_chat.cpp
  ↓
OpenRouter API
  ↓
chat_result.json
  ↓
GitHub Pages deploy
  ↓
open page polls every 5 seconds and displays reply
```

> Important limitation: a public GitHub Pages site cannot safely write to your GitHub repository or trigger a protected workflow without credentials. Therefore the only manual bridge is uploading/replacing the downloaded JSON file and committing it. After that commit, the matching GitHub Action runs automatically.

---

## Project files

```text
CareMate_AI_FINAL_GitHub_Actions/
├── index.html
├── README.md
├── .nojekyll
├── .gitignore
│
├── cpp/
│   ├── common.hpp
│   ├── health_analysis.cpp
│   └── ai_chat.cpp
│
├── data/
│   ├── patient.json
│   ├── history.json
│   ├── health_input.json
│   ├── health_result.json
│   ├── chat_input.json
│   └── chat_result.json
│
└── .github/
    └── workflows/
        ├── deploy-pages.yml
        ├── health-analysis.yml
        └── ai-chat.yml
```

---

# One-time GitHub setup

## 1. Create a GitHub repository

Create a repository and upload **all files/folders inside this package to the repository root**.

The branch should be:

```text
main
```

Do not upload your API keys as files.

---

## 2. Enable GitHub Pages

Go to:

```text
Repository
→ Settings
→ Pages
→ Build and deployment
→ Source
→ GitHub Actions
```

The included `deploy-pages.yml` publishes the website.

The Gemini/OpenRouter analysis jobs intentionally skip the repository's very first branch-creation push, so uploading the starter package does not immediately consume API calls. Subsequent changes to the two input JSON files trigger them normally.

After deployment, your URL will look similar to:

```text
https://YOUR_USERNAME.github.io/YOUR_REPOSITORY/
```

---

## 3. Add Gemini secret

Go to:

```text
Settings
→ Secrets and variables
→ Actions
→ Secrets
→ New repository secret
```

Create:

```text
Name: GEMINI_API_KEY
Value: your Gemini API key
```

The C++ program reads the key only from the GitHub Actions environment.

### Optional Gemini model variable

Go to:

```text
Settings
→ Secrets and variables
→ Actions
→ Variables
```

Create:

```text
GEMINI_MODEL = gemini-3.5-flash
```

If you do not create this variable, the current C++ default is:

```text
gemini-3.5-flash
```

---

## 4. Add OpenRouter secret

Create another GitHub Actions secret:

```text
Name: OPENROUTER_API_KEY
Value: your OpenRouter API key
```

### Optional OpenRouter model variable

Create:

```text
OPENROUTER_MODEL
```

For a simple low-cost prototype you can use:

```text
openrouter/free
```

The C++ default is also:

```text
openrouter/free
```

You can later replace it with any OpenRouter model ID you prefer.

---

## 5. GitHub Actions write permission

The workflow files explicitly request `contents: write`.

If the workflow's `git push` step returns a 403 permission error, go to:

```text
Settings
→ Actions
→ General
→ Workflow permissions
→ Read and write permissions
```

Then save.

---

# How to demo Health Check

## Step 1 — Open the CareMate page

Go to:

```text
Health Check
```

Enter Mary's data.

Example:

```text
Systolic: 145
Diastolic: 92
Blood glucose: 7.1
Heart rate: 78
Symptom: Dizziness
Note: I felt dizzy after breakfast.
```

Click:

```text
Confirm Health Check
```

The browser downloads:

```text
health_input.json
```

The webpage stores the request ID locally and starts checking for the new result every 5 seconds.

## Step 2 — Upload the file to GitHub

In GitHub open:

```text
data/
```

Upload/replace the downloaded file as:

```text
data/health_input.json
```

Commit the change to `main`.

## Step 3 — Everything else is automatic

Because `data/health_input.json` changed, GitHub automatically starts:

```text
CareMate Health Analysis - Gemini
```

The workflow:

1. installs `g++`, `libcurl` and `nlohmann/json`;
2. compiles `cpp/health_analysis.cpp`;
3. reads `patient.json`, `history.json` and `health_input.json`;
4. runs a local C++ emergency-wording safety check;
5. calls the Gemini API for normal trend analysis;
6. writes `health_result.json`;
7. appends the new record to `history.json`;
8. commits the generated files;
9. deploys the updated GitHub Pages site.

When the published `health_result.json` matches the request ID generated by your browser, the open CareMate page automatically updates:

```text
Dashboard
Alerts & Trends
Health Report
```

No separate manual **Run workflow** click is needed.

---

# How to demo AI Assistant

The AI Assistant is deliberately separate from Health Check.

## Step 1 — Ask a question

Go to:

```text
AI Assistant
```

Example:

```text
Should I contact my GP?
```

Click:

```text
Ask AI
```

The browser downloads:

```text
chat_input.json
```

## Step 2 — Upload it

Replace:

```text
data/chat_input.json
```

in GitHub and commit.

## Step 3 — OpenRouter runs separately

GitHub automatically starts:

```text
CareMate AI Assistant - OpenRouter
```

The C++ program reads:

```text
patient.json
history.json
health_result.json
chat_input.json
```

and calls OpenRouter.

It writes:

```text
chat_result.json
```

Then the workflow redeploys GitHub Pages.

The open AI Assistant page polls for the matching request ID and displays the new answer automatically.

---

# API responsibilities

## Gemini

Used only for:

```text
Health Check
→ trend analysis
→ health status
→ reason
→ next action
→ care summary
```

## OpenRouter

Used only for:

```text
AI Assistant questions
→ explain saved records
→ explain recent Gemini analysis
→ answer care-plan questions
→ prepare simple family/care summaries
```

This means one Health Check normally causes one Gemini request, while an AI Assistant question causes a separate OpenRouter request.

---

# C++ safety layer

Before the external AI call, the C++ code checks a small list of emergency-related symptom phrases.

This is intentionally a narrow prototype rule. It does **not** diagnose based on blood-pressure or blood-glucose numbers.

If emergency-related wording is found, CareMate skips the ordinary model analysis and gives an urgent-professional-help message.

Both AI prompts also tell the model:

- no diagnosis;
- no medication changes;
- no claim that one reading proves a disease;
- use clear, calm language;
- recommend qualified professional review when appropriate;
- never delay urgent professional care.

This is a student prototype and must not be used for real clinical decisions.

---

# Testing without new API calls

The package already contains sample:

```text
health_result.json
chat_result.json
history.json
```

so the page has something to display before you configure API keys.

When you run the workflows with valid secrets, those sample result files are replaced.

---

# Troubleshooting

## Health workflow says `GEMINI_API_KEY missing`

Create the GitHub Actions secret exactly as:

```text
GEMINI_API_KEY
```

## Chat workflow says `OPENROUTER_API_KEY missing`

Create:

```text
OPENROUTER_API_KEY
```

## Git push returns 403

Check:

```text
Settings → Actions → General → Workflow permissions
```

and allow read/write if your repository policy requires it.

## Page still shows the previous answer

The browser polls every 5 seconds, but GitHub Pages still needs time to deploy the new artifact. Keep the page open and wait for the workflow's **deploy** job to finish.

## OpenRouter free model is busy

Set a repository variable:

```text
OPENROUTER_MODEL
```

to another model ID available in your OpenRouter account.

## Do not use real private health data

Mary Tan is a synthetic design-sprint persona. Keep the GitHub demo data synthetic.
