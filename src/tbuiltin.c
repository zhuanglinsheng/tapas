#include "tapas/tbuiltin.h"

#include <stddef.h>

const char *tbuiltin_name(tbuiltin_id type)
{
	static const char *const names[tbuiltin_count] = {
		"AnyType", "Nil", "Bool", "Int", "Float", "String", "List",
		"Pair", "Dictionary", "Iterator", "Function", "Library",
		"RealArray", "BoolArray", "Time", "Type", "Indexable",
		"IndexSettable", "Appendable", "Deletable", "Contains",
		"Iterable", "Rule", "RuleInstance", "RuleIR", "Parameter",
		"Capture", "Item", "Condition", "Requirement", "Term",
		"Origin", "CheckResult", "Violation", "RuleDiagnostic",
		"Evaluator", "Context", "Result", "EvaluatorDiagnostic"
	};
	return type >= 0 && type < tbuiltin_count ? names[type] : nullptr;
}
