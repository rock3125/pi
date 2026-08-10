package nz.pi.piview

/**
 * Turns pi's terminal output back into structure.
 *
 * pi answers in the same prose it prints at the prompt, so the shape of an
 * answer has to be recovered rather than read off a field.  Two facts about the
 * interpreter make that reliable:
 *
 *   - a query's output string is built in one order - anything write/1 printed,
 *     then one `Var=value` line per binding per solution, then "yes" if there
 *     were no bindings at all (query.cpp, `Query::ExecuteQuery`);
 *   - solutions are emitted back to back with no separator, but a solution
 *     binds each variable at most once (engine.cpp, `Engine::GatherResults`),
 *     so a repeated variable name is where the next solution starts.
 *
 * Everything else - the timing line, "no", the error prefixes - is a literal
 * the interpreter prints verbatim.
 */
object Parser {

    private val BINDING = Regex("""^([A-Z_][A-Za-z0-9_]*)=(.*)$""")
    private val EXEC_TIME = Regex("""^\(execution time\s+([0-9.eE+-]+)\s+seconds\)$""")
    private val LISTING = Regex("""^(\d+)\s{2,}(.*)$""")

    private val ERROR_PREFIXES = listOf(
        "error parsing query:",
        "error parsing statement(s):",
        "error parsing statements:",
        "could not load database file",
        "usage:",
        "line too long",
        "piview:",
    )

    // ------------------------------------------------------------ queries

    fun parseQuery(goal: String, raw: String): QueryResult {
        val lines = raw.split("\n").toMutableList()

        // the timing line is always last when the goal actually ran
        var seconds: Double? = null
        var i = lines.indexOfLast { it.isNotBlank() }
        if (i >= 0) {
            val match = EXEC_TIME.find(lines[i].trim())
            if (match != null) {
                seconds = match.groupValues[1].toDoubleOrNull()
                lines.subList(i, lines.size).clear()
            }
        }

        val body = lines.dropLastWhile { it.isBlank() }
        val trimmed = body.joinToString("\n").trim()

        errorIn(trimmed)?.let {
            return QueryResult(goal, Outcome.ERROR, error = it, executionSeconds = seconds, raw = raw)
        }

        if (trimmed == "no") {
            return QueryResult(goal, Outcome.FAILURE, executionSeconds = seconds, raw = raw)
        }

        // an answer with no bindings ends in a bare "yes"
        val withoutYes = if (body.isNotEmpty() && body.last().trim() == "yes") body.dropLast(1) else body
        val hadYes = withoutYes.size != body.size

        // walk back over the binding lines; whatever sits in front of them is
        // program output
        var split = withoutYes.size
        while (split > 0 && BINDING.matches(withoutYes[split - 1])) split--
        val output = withoutYes.take(split).joinToString("\n").trimEnd()
        val bindingLines = withoutYes.drop(split)

        if (bindingLines.isEmpty() && !hadYes) {
            // nothing recognisable came back - report it rather than inventing
            // a success
            return if (trimmed.isEmpty()) {
                QueryResult(goal, Outcome.SUCCESS, output = output, executionSeconds = seconds, raw = raw)
            } else {
                QueryResult(
                    goal, Outcome.SUCCESS,
                    solutions = listOf(Solution(emptyMap())),
                    output = trimmed, executionSeconds = seconds, raw = raw,
                )
            }
        }

        val solutions = groupSolutions(bindingLines)
        val variables = LinkedHashSet<String>().apply {
            solutions.forEach { addAll(it.bindings.keys) }
        }.toList()

        return QueryResult(
            goal = goal,
            outcome = Outcome.SUCCESS,
            variables = variables,
            solutions = solutions.ifEmpty { listOf(Solution(emptyMap())) },
            output = output,
            executionSeconds = seconds,
            raw = raw,
        )
    }

    /** a repeated variable name starts the next solution */
    private fun groupSolutions(bindingLines: List<String>): List<Solution> {
        val solutions = mutableListOf<Solution>()
        var current = LinkedHashMap<String, String>()

        for (line in bindingLines) {
            val match = BINDING.find(line) ?: continue
            val name = match.groupValues[1]
            val value = match.groupValues[2]
            if (current.containsKey(name)) {
                solutions += Solution(current)
                current = LinkedHashMap()
            }
            current[name] = value
        }
        if (current.isNotEmpty()) solutions += Solution(current)
        return solutions
    }

    // ------------------------------------------------------------ the database

    fun parseListing(raw: String): DatabaseSnapshot {
        val trimmed = raw.trim()
        errorIn(trimmed)?.let { return DatabaseSnapshot(error = it, revision = "error:$it") }
        if (trimmed == "database empty" || trimmed.isEmpty()) {
            return DatabaseSnapshot(revision = "empty")
        }

        val clauses = raw.split("\n").mapNotNull { line ->
            val match = LISTING.find(line.trimEnd()) ?: return@mapNotNull null
            val index = match.groupValues[1].toIntOrNull() ?: return@mapNotNull null
            val text = match.groupValues[2].removeSuffix(".")
            toClause(index, text)
        }

        val predicates = clauses
            .groupBy { it.predicate }
            .map { (indicator, group) ->
                Predicate(
                    name = group.first().name,
                    arity = group.first().arity,
                    indicator = indicator,
                    clauses = group,
                )
            }
            .sortedBy { it.clauses.first().index }

        return DatabaseSnapshot(
            clauses = clauses,
            predicates = predicates,
            revision = clauses.joinToString("") { "${it.index}=${it.text}" }.hashCode().toString(),
        )
    }

    private fun toClause(index: Int, text: String): Clause {
        val neck = topLevelNeck(text)
        val head = (if (neck >= 0) text.substring(0, neck) else text).trim()
        val (name, arity) = headIndicator(head)
        return Clause(
            index = index,
            text = text,
            predicate = "$name/$arity",
            name = name,
            arity = arity,
            isRule = neck >= 0,
        )
    }

    /** index of the `:-` that separates head from body, or -1 for a fact */
    private fun topLevelNeck(text: String): Int {
        var depth = 0
        var quoted = false
        var i = 0
        while (i < text.length) {
            val c = text[i]
            when {
                quoted -> if (c == '\'') quoted = false
                c == '\'' -> quoted = true
                c == '(' || c == '[' -> depth++
                c == ')' || c == ']' -> depth--
                depth == 0 && c == ':' && i + 1 < text.length && text[i + 1] == '-' -> return i
            }
            i++
        }
        return -1
    }

    private fun headIndicator(head: String): Pair<String, Int> {
        val open = head.indexOf('(')
        if (open <= 0 || !head.endsWith(")")) {
            // a bare atom, or something the parser rendered as an operator - in
            // either case there is nothing to count
            return head.trim().ifEmpty { "?" } to 0
        }
        val name = head.substring(0, open).trim()
        val args = head.substring(open + 1, head.length - 1)
        return name to countArguments(args)
    }

    private fun countArguments(args: String): Int {
        if (args.isBlank()) return 0
        var depth = 0
        var quoted = false
        var count = 1
        for (c in args) {
            when {
                quoted -> if (c == '\'') quoted = false
                c == '\'' -> quoted = true
                c == '(' || c == '[' -> depth++
                c == ')' || c == ']' -> depth--
                c == ',' && depth == 0 -> count++
            }
        }
        return count
    }

    // ------------------------------------------------------------ everything else

    /** assert, delete, load, new, help - anything that is not a query */
    fun parseCommand(command: String, raw: String): CommandResult {
        val trimmed = raw.trim()
        val error = errorIn(trimmed)
        return CommandResult(
            command = command,
            ok = error == null,
            message = error ?: trimmed.ifEmpty { "ok" },
            raw = raw,
        )
    }

    private fun errorIn(text: String): String? {
        val first = text.lineSequence().firstOrNull { it.isNotBlank() }?.trim() ?: return null
        return if (ERROR_PREFIXES.any { first.startsWith(it) }) text else null
    }

    // ------------------------------------------------------------ input handling

    /**
     * Splits pasted prolog into one statement per line.
     *
     * pi's protocol is line oriented, but a clause is terminated by `.` and may
     * be written across as many lines as it likes.  Comments run to the end of a
     * line, so they have to go before the lines are joined or they would swallow
     * the code that follows.
     */
    fun splitProgram(source: String): List<String> {
        val stripped = buildString {
            var quoted = false
            var i = 0
            while (i < source.length) {
                val c = source[i]
                when {
                    quoted -> {
                        append(c)
                        if (c == '\'') quoted = false
                    }
                    c == '\'' -> { append(c); quoted = true }
                    c == '%' -> { while (i < source.length && source[i] != '\n') i++; continue }
                    else -> append(c)
                }
                i++
            }
        }

        val statements = mutableListOf<String>()
        val current = StringBuilder()
        var depth = 0
        var quoted = false

        for (c in stripped) {
            when {
                quoted -> {
                    current.append(c)
                    if (c == '\'') quoted = false
                }
                c == '\'' -> { current.append(c); quoted = true }
                c == '(' || c == '[' -> { depth++; current.append(c) }
                c == ')' || c == ']' -> { depth--; current.append(c) }
                c == '.' && depth == 0 -> {
                    current.append(c)
                    statements += current.toString()
                    current.setLength(0)
                }
                c == '\n' || c == '\r' || c == '\t' -> current.append(' ')
                else -> current.append(c)
            }
        }
        if (current.isNotBlank()) statements += current.toString()

        return statements.map { it.trim().replace(Regex("\\s+"), " ") }.filter { it.isNotBlank() }
    }

    /** does this look like a query rather than a clause to assert? */
    fun isQuery(text: String): Boolean = text.trimStart().startsWith("?")

    /** put the `?-` and the `.` on a goal if the caller left them off */
    fun normaliseGoal(goal: String): String {
        var g = goal.trim()
        if (!g.startsWith("?")) g = "?- $g"
        if (!g.endsWith(".")) g = "$g."
        return g
    }

    /** put the `.` on a clause if the caller left it off */
    fun normaliseClause(clause: String): String {
        val c = clause.trim()
        return if (c.endsWith(".")) c else "$c."
    }
}
