#include "common.hpp"
#include <cstdlib>
#include <iostream>

static bool emergency_question(const std::string& question) {
    const std::vector<std::string> terms = {
        "severe chest pain",
        "severe difficulty breathing",
        "cannot breathe",
        "can't breathe",
        "loss of consciousness",
        "unconscious",
        "collapsed",
        "collapse"
    };
    return contains_any(question, terms);
}

static json local_urgent_chat(const json& input, const std::string& model) {
    return {
        {"requestId", input.value("requestId", std::string{})},
        {"generatedAt", iso_utc_now()},
        {"provider", "C++ safety layer"},
        {"model", model},
        {"question", input.value("question", std::string{})},
        {"reply", "CareMate cannot assess an emergency. Severe or rapidly worsening symptoms require appropriate urgent professional medical help rather than waiting for an AI response."},
        {"source", "Local safety rule"}
    };
}

static json error_chat(const json& input, const std::string& model, const std::string& message) {
    return {
        {"requestId", input.value("requestId", std::string{})},
        {"generatedAt", iso_utc_now()},
        {"provider", "OpenRouter"},
        {"model", model},
        {"question", input.value("question", std::string{})},
        {"reply", "The OpenRouter AI request could not be completed. Check the GitHub Actions log and OPENROUTER_API_KEY, then run the chat workflow again."},
        {"error", message},
        {"source", "C++ error fallback"}
    };
}

static json call_openrouter(
    const std::string& api_key,
    const std::string& model,
    const json& patient,
    const json& history,
    const json& health_result,
    const json& input
) {
    const std::string system_prompt =
R"(You are CareMate AI, a conversational assistant in a university design-sprint prototype for an older adult.

Use only the supplied synthetic persona context and saved health records when discussing the user's personal trend.
Safety boundaries:
- Do not diagnose diseases.
- Do not prescribe, stop, change, or adjust medication.
- Do not claim that one number proves a condition.
- Explain trends and care-plan information in simple, calm language.
- If symptoms are concerning, persistent, worsening, or the user is worried, suggest appropriate review by a qualified healthcare professional.
- Never advise delaying urgent professional care.
- If a question is outside CareMate's health-monitoring scope, say what CareMate can help with instead.
- Keep the answer concise and easy to understand.)";

    const std::string context =
        "PATIENT PROFILE:\n" + patient.dump(2) +
        "\n\nRECENT HOME RECORDS:\n" + history.dump(2) +
        "\n\nLATEST GEMINI HEALTH ANALYSIS:\n" + health_result.dump(2) +
        "\n\nUSER QUESTION:\n" + input.value("question", std::string{});

    json body = {
        {"model", model},
        {"messages", json::array({
            {{"role","system"},{"content",system_prompt}},
            {{"role","user"},{"content",context}}
        })},
        {"temperature",0.3},
        {"max_tokens",500}
    };

    const HttpResponse http = post_json(
        "https://openrouter.ai/api/v1/chat/completions",
        body,
        {
            "Authorization: Bearer " + api_key,
            "X-Title: CareMate AI Design Sprint"
        }
    );

    json response;
    try {
        response = json::parse(http.body);
    } catch (...) {
        throw std::runtime_error("OpenRouter returned a non-JSON HTTP response.");
    }

    if (http.status < 200 || http.status >= 300) {
        const std::string api_message = json_error_message(response);
        throw std::runtime_error(
            "OpenRouter API HTTP " + std::to_string(http.status) +
            (api_message.empty() ? "" : ": " + api_message)
        );
    }

    if (!response.contains("choices") || !response["choices"].is_array() ||
        response["choices"].empty() ||
        !response["choices"][0].contains("message") ||
        !response["choices"][0]["message"].contains("content")) {
        throw std::runtime_error("OpenRouter response did not contain choices[0].message.content.");
    }

    std::string reply;
    const auto& content = response["choices"][0]["message"]["content"];
    if (content.is_string()) {
        reply = content.get<std::string>();
    } else if (content.is_array()) {
        for (const auto& part : content) {
            if (part.is_object() && part.value("type", std::string{}) == "text" &&
                part.contains("text") && part["text"].is_string()) {
                if (!reply.empty()) reply += "\n";
                reply += part["text"].get<std::string>();
            }
        }
    }

    if (reply.empty()) throw std::runtime_error("OpenRouter returned an empty assistant reply.");

    return {
        {"requestId", input.value("requestId", std::string{})},
        {"generatedAt", iso_utc_now()},
        {"provider", "OpenRouter"},
        {"model", response.value("model", model)},
        {"question", input.value("question", std::string{})},
        {"reply", reply},
        {"source", "OpenRouter API via C++ and GitHub Actions"}
    };
}

int main() {
    try {
        const json patient = read_json_file("data/patient.json");
        const json history = read_json_file("data/history.json");
        const json health_result = read_json_file("data/health_result.json");
        const json input = read_json_file("data/chat_input.json");

        const char* key_env = std::getenv("OPENROUTER_API_KEY");
        const char* model_env = std::getenv("OPENROUTER_MODEL");
        const std::string api_key = key_env ? key_env : "";
        const std::string model =
            (model_env && std::string(model_env).size())
            ? std::string(model_env)
            : "openrouter/free";

        if (emergency_question(input.value("question", std::string{}))) {
            write_json_file("data/chat_result.json", local_urgent_chat(input, model));
            std::cout << "Local emergency-wording chat safety rule used.\n";
            return 0;
        }

        if (api_key.empty()) {
            write_json_file(
                "data/chat_result.json",
                error_chat(input, model, "OPENROUTER_API_KEY is not configured in GitHub Actions secrets.")
            );
            std::cout << "OPENROUTER_API_KEY missing; fallback chat result written.\n";
            return 0;
        }

        try {
            const json result = call_openrouter(api_key, model, patient, history, health_result, input);
            write_json_file("data/chat_result.json", result);
            std::cout << "OpenRouter chat completed using " << model << ".\n";
        } catch (const std::exception& e) {
            write_json_file("data/chat_result.json", error_chat(input, model, e.what()));
            std::cout << "OpenRouter call failed; fallback chat result written: " << e.what() << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal chat workflow error: " << e.what() << "\n";
        return 1;
    }
}
