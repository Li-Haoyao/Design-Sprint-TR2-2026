#include "common.hpp"
#include <cstdlib>
#include <iostream>

static bool emergency_phrase_detected(const json& input) {
    const std::string combined =
        input.value("symptom", std::string{}) + " " +
        input.value("note", std::string{});

    // Deliberately small prototype safety layer: symptom wording only.
    // It does not diagnose or classify numeric vital-sign values.
    const std::vector<std::string> emergency_terms = {
        "severe chest pain",
        "severe difficulty breathing",
        "cannot breathe",
        "can't breathe",
        "loss of consciousness",
        "unconscious",
        "collapse",
        "collapsed",
        "face drooping",
        "slurred speech",
        "one-sided weakness"
    };
    return contains_any(combined, emergency_terms);
}

static void append_history_once(json& history, const json& input) {
    if (!history.is_array()) history = json::array();

    const std::string request_id = input.value("requestId", std::string{});
    bool exists = false;
    if (!request_id.empty()) {
        for (const auto& row : history) {
            if (row.value("requestId", std::string{}) == request_id) {
                exists = true;
                break;
            }
        }
    }

    if (!exists) history.push_back(input);

    // Keep this prototype file small.
    while (history.size() > 30) history.erase(history.begin());
}

static json local_urgent_result(const json& input) {
    return {
        {"requestId", input.value("requestId", std::string{})},
        {"generatedAt", iso_utc_now()},
        {"provider", "C++ safety layer"},
        {"model", "local-rule"},
        {"status", "Urgent"},
        {"headline", "Urgent professional help recommended"},
        {"meaningfulChanges", 1},
        {"summary", "Potentially serious symptom wording was entered. CareMate does not ask an AI model to assess an emergency."},
        {"reason", "The local C++ safety layer detected emergency-related symptom wording before the Gemini request."},
        {"secondaryFinding", "Numeric readings are not used by this prototype to diagnose an emergency."},
        {"nextActionShort", "Seek urgent help"},
        {"nextAction", "Use appropriate urgent professional medical services rather than waiting for an app or AI response."},
        {"careSummary", "Urgent symptom wording was recorded. This student prototype does not diagnose or manage emergencies."},
        {"familySummary", "Mary entered symptom wording that requires urgent professional attention rather than app-based assessment."},
        {"symptomGuidance", "Do not rely on CareMate for emergency assessment."},
        {"aiExplanation", "The C++ safety layer intercepted this submission before the external AI API."},
        {"source", "Local safety rule"}
    };
}

static json error_result(const json& input, const std::string& message, const std::string& model) {
    return {
        {"requestId", input.value("requestId", std::string{})},
        {"generatedAt", iso_utc_now()},
        {"provider", "Gemini"},
        {"model", model},
        {"status", "Monitor"},
        {"headline", "AI analysis unavailable"},
        {"meaningfulChanges", 0},
        {"summary", "The health record was saved, but the Gemini analysis could not be completed."},
        {"reason", message},
        {"secondaryFinding", "Check the GitHub Actions log and API configuration."},
        {"nextActionShort", "Check workflow"},
        {"nextAction", "Check GEMINI_API_KEY and the workflow log, then run the health workflow again."},
        {"careSummary", "No new Gemini care summary was generated because the API request did not complete."},
        {"familySummary", "No new Gemini-generated family summary is available yet."},
        {"symptomGuidance", "For concerning, persistent, or worsening symptoms, contact a qualified healthcare professional rather than relying on this prototype."},
        {"aiExplanation", "GitHub Actions completed the file-processing step but the Gemini API call was unavailable."},
        {"source", "C++ error fallback"}
    };
}

static std::string extract_gemini_text(const json& response) {
    if (!response.contains("candidates") || !response["candidates"].is_array() || response["candidates"].empty())
        return "";

    const auto& candidate = response["candidates"][0];
    if (!candidate.contains("content") || !candidate["content"].contains("parts") ||
        !candidate["content"]["parts"].is_array())
        return "";

    std::string text;
    for (const auto& part : candidate["content"]["parts"]) {
        if (part.contains("text") && part["text"].is_string()) {
            if (!text.empty()) text += "\n";
            text += part["text"].get<std::string>();
        }
    }
    return text;
}

static json call_gemini(
    const std::string& api_key,
    const std::string& model,
    const json& patient,
    const json& history,
    const json& input
) {
    const std::string prompt =
R"(You are CareMate AI, a preventive health-monitoring assistant for a university design-sprint prototype.

Analyse the synthetic persona's recorded home-health pattern. Follow these boundaries:
- Do not diagnose a disease or claim that a single value proves a medical condition.
- Do not prescribe, stop, change, or adjust medication.
- Compare the newest entry with the supplied recent records and explain meaningful changes in calm, simple language suitable for a 70-year-old user.
- Treat the data as a prototype record, not a substitute for clinical assessment.
- If follow-up is appropriate, say that a qualified healthcare professional can review the trend.
- If symptoms are concerning, persistent, worsening, or the user is worried, recommend appropriate professional help.
- Never tell the user to delay urgent professional care.
- Keep the output concise.

Return the requested JSON fields only.)"
        + std::string("\n\nPATIENT PROFILE:\n") + patient.dump(2)
        + "\n\nRECENT RECORDS (includes latest submission):\n" + history.dump(2)
        + "\n\nLATEST SUBMISSION:\n" + input.dump(2);

    json response_schema = {
        {"type","OBJECT"},
        {"properties",{
            {"status",{{"type","STRING"},{"enum",json::array({"Routine","Monitor","Urgent"})}}},
            {"headline",{{"type","STRING"}}},
            {"meaningfulChanges",{{"type","INTEGER"}}},
            {"summary",{{"type","STRING"}}},
            {"reason",{{"type","STRING"}}},
            {"secondaryFinding",{{"type","STRING"}}},
            {"nextActionShort",{{"type","STRING"}}},
            {"nextAction",{{"type","STRING"}}},
            {"careSummary",{{"type","STRING"}}},
            {"familySummary",{{"type","STRING"}}},
            {"symptomGuidance",{{"type","STRING"}}},
            {"aiExplanation",{{"type","STRING"}}}
        }},
        {"required",json::array({
            "status","headline","meaningfulChanges","summary","reason",
            "secondaryFinding","nextActionShort","nextAction",
            "careSummary","familySummary","symptomGuidance","aiExplanation"
        })}
    };

    json body = {
        {"contents", json::array({
            {
                {"role","user"},
                {"parts",json::array({{{"text",prompt}}})}
            }
        })},
        {"generationConfig",{
            {"temperature",0.2},
            {"response_mime_type","application/json"},
            {"response_schema",response_schema}
        }}
    };

    const std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/" +
        model + ":generateContent";

    const HttpResponse http = post_json(
        url,
        body,
        {"x-goog-api-key: " + api_key}
    );

    json envelope;
    try {
        envelope = json::parse(http.body);
    } catch (...) {
        throw std::runtime_error("Gemini returned a non-JSON HTTP response.");
    }

    if (http.status < 200 || http.status >= 300) {
        const std::string api_message = json_error_message(envelope);
        throw std::runtime_error(
            "Gemini API HTTP " + std::to_string(http.status) +
            (api_message.empty() ? "" : ": " + api_message)
        );
    }

    const std::string model_text = extract_gemini_text(envelope);
    if (model_text.empty()) throw std::runtime_error("Gemini response did not contain output text.");

    json result;
    try {
        result = json::parse(model_text);
    } catch (...) {
        throw std::runtime_error("Gemini output was not valid structured JSON.");
    }

    result["requestId"] = input.value("requestId", std::string{});
    result["generatedAt"] = iso_utc_now();
    result["provider"] = "Gemini";
    result["model"] = model;
    result["source"] = "Gemini API via C++ and GitHub Actions";
    return result;
}

int main() {
    try {
        const json patient = read_json_file("data/patient.json");
        const json input = read_json_file("data/health_input.json");
        json history = read_json_file("data/history.json");

        append_history_once(history, input);
        write_json_file("data/history.json", history);

        if (emergency_phrase_detected(input)) {
            write_json_file("data/health_result.json", local_urgent_result(input));
            std::cout << "Local emergency-wording safety rule used.\n";
            return 0;
        }

        const char* key_env = std::getenv("GEMINI_API_KEY");
        const char* model_env = std::getenv("GEMINI_MODEL");
        const std::string api_key = key_env ? key_env : "";
        const std::string model =
            (model_env && std::string(model_env).size())
            ? std::string(model_env)
            : "gemini-3.5-flash";

        if (api_key.empty()) {
            write_json_file(
                "data/health_result.json",
                error_result(input, "GEMINI_API_KEY is not configured in GitHub Actions secrets.", model)
            );
            std::cout << "GEMINI_API_KEY missing; fallback result written.\n";
            return 0;
        }

        try {
            const json result = call_gemini(api_key, model, patient, history, input);
            write_json_file("data/health_result.json", result);
            std::cout << "Gemini health analysis completed using " << model << ".\n";
        } catch (const std::exception& e) {
            write_json_file("data/health_result.json", error_result(input, e.what(), model));
            std::cout << "Gemini call failed; fallback result written: " << e.what() << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal health workflow error: " << e.what() << "\n";
        return 1;
    }
}
