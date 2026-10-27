// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

class CountingTool : public Tool {
public:
    CountingTool(std::string n, int* destructions)
        : n_(std::move(n)), destructions_(destructions), schema_(JsonValue::object()) {}
    ~CountingTool() override { ++*destructions_; }
    std::string name() const override { return n_; }
    std::string description() const override { return "counted test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override {
        return ToolResult::success(n_);
    }
private:
    std::string n_;
    int*        destructions_;
    JsonValue   schema_;
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- registry

TEST(reg_empty_and_null_rejected) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1);
    CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    CHECK(r.find("absent") == nullptr);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));

    std::vector<std::string> names;
    r.for_each([&](const Tool& tool) { names.push_back(tool.name()); });
    std::sort(names.begin(), names.end());
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "alpha");
    CHECK(names[1] == "beta");
    CHECK(names[2] == "gamma");
}

TEST(reg_ownership_destroys_exactly_once) {
    int destructions = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountingTool>("same", &destructions)));
        CHECK(!r.add(std::make_unique<CountingTool>("same", &destructions)));
        CHECK(destructions == 1);
        CHECK(r.size() == 1);
    }
    CHECK(destructions == 2);
}

TEST(reg_hash_is_order_sensitive_and_full_width) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1") != ToolRegistry::hash("tool_2"));

    std::unordered_set<std::size_t> hashes;
    for (int i = 0; i < 10000; ++i) {
        hashes.insert(ToolRegistry::hash("tool_" + std::to_string(i)));
    }
    CHECK(hashes.size() >= 9990);
}

TEST(reg_10000_tools_and_weak_hash_comparison) {
    ToolRegistry r;
    Tool* first = nullptr;
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        auto tool = std::make_unique<NamedTool>(name);
        if (i == 0) first = tool.get();
        REQUIRE(r.add(std::move(tool)));
        CHECK(r.size() == static_cast<std::size_t>(i + 1));
        CHECK(r.load_factor() <= 1.0);
    }

    for (int i = 0; i < 10000; ++i) {
        CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    }
    CHECK(r.find("tool_0") == first);
    CHECK(r.max_probe_length() <= 64);

    std::vector<int> weak_counts(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        // The weak hash puts thousands of these similar names together.
        const std::size_t weak_hash =
            static_cast<unsigned char>(name[0]) + name.size();
        ++weak_counts[weak_hash % r.bucket_count()];
    }
    const int weak_longest = *std::max_element(weak_counts.begin(), weak_counts.end());
    std::printf("registry: buckets=%zu load=%.6f max_chain=%zu weak_max_chain=%d\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), weak_longest);
    CHECK(weak_longest > static_cast<int>(r.max_probe_length()));
}

// ------------------------------------------------------------- workspace

TEST(ws_accepts_and_normalizes_required_paths) {
    TempDir d;
    Workspace ws(d.path());
    const std::vector<std::pair<std::string, fs::path>> cases = {
        {"notes/todo.txt", ws.root() / "notes" / "todo.txt"},
        {"./notes//todo.txt", ws.root() / "notes" / "todo.txt"},
        {".", ws.root()},
        {"a/../b", ws.root() / "b"},
        {"a/..", ws.root()},
        {"new/dir/file.txt", ws.root() / "new" / "dir" / "file.txt"},
        {"my..notes.txt", ws.root() / "my..notes.txt"},
    };

    for (const auto& test_case : cases) {
        fs::path out;
        std::string reason = "stale";
        REQUIRE(ws.resolve(test_case.first, &out, &reason));
        CHECK(out == test_case.second);
        CHECK(reason.empty());
    }
}

TEST(ws_rejects_empty_and_embedded_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;

    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");

    const std::string with_nul = std::string("notes") + '\0' + "/x";
    reason.clear();
    CHECK(!ws.resolve(with_nul, &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_absolute_path) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_all_escape_variants) {
    TempDir d;
    Workspace ws(d.path());
    const std::vector<std::string> paths = {
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../ws/notes/todo.txt",
    };
    for (const std::string& path : paths) {
        fs::path out;
        std::string reason;
        CHECK(!ws.resolve(path, &out, &reason));
        CHECK(!reason.empty());
    }
}

// ------------------------------------------------------------ calculator

TEST(calc_left_to_right_subtraction) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3.0);
}

TEST(calc_left_to_right_division) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2.0);
}

TEST(calc_parentheses_override_precedence) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20.0);
}

TEST(calc_nested_expression) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9.0);
}

TEST(calc_negative_numbers) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9.0);
}

TEST(calc_decimal_division) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
}

TEST(calc_division_by_parenthesized_zero) {
    double v = 0.0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_original_expressions) {
    double v = 0.0;
    std::string err;
    REQUIRE(eval("8 * 3 + 2 * 5", &v, &err));
    CHECK(v == 34.0);
    REQUIRE(eval("((1 + 2) * (6 - 2)) / 3", &v, &err));
    CHECK(v == 4.0);
}

TEST(calc_handles_very_long_expression) {
    // Check that a long expression doesn't slow the evaluator down too much.
    std::string expression = "1";
    expression.reserve(80000);
    for (int i = 1; i < 20000; ++i) expression += "+1";

    double v = 0.0;
    std::string err;
    REQUIRE(eval(expression, &v, &err));
    CHECK(v == 20000.0);
}

TEST(calc_handles_deep_parentheses_iteratively) {
    // Check that nested parentheses don't cause recursive stack problems.
    std::string expression(5000, '(');
    expression += "7";
    expression.append(5000, ')');

    double v = 0.0;
    std::string err;
    REQUIRE(eval(expression, &v, &err));
    CHECK(v == 7.0);
}

TEST(calc_malformed_tokens_return_errors) {
    double v = 0.0;
    std::string err;
    std::vector<CalcToken> tokens;
    CHECK(!calc_evaluate(tokens, &v, &err));
    CHECK(!err.empty());

    tokens = {{CalcToken::Kind::Number, 1.0}, {CalcToken::Kind::Plus, 0.0}};
    err.clear();
    CHECK(!calc_evaluate(tokens, &v, &err));
    CHECK(!err.empty());

    tokens = {{CalcToken::Kind::Number, 1.0},
              {CalcToken::Kind::Number, 2.0},
              {CalcToken::Kind::Plus, 0.0}};
    err.clear();
    CHECK(!calc_evaluate(tokens, &v, &err));
    CHECK(!err.empty());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
