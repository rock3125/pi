package nz.pi.piview

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The strings below are verbatim answers from a running `./prolog --port`,
 * captured off the socket. If pi's output ever changes shape, these are what
 * should fail first.
 */
class ParserTest {

    @Test
    fun `one variable, several solutions`() {
        val raw = "X=peter\nX=mark\nX=micheal\nX=jj\n\n(execution time 0.00002534 seconds)\n"
        val result = Parser.parseQuery("?- father(fred,X).", raw)

        assertEquals(Outcome.SUCCESS, result.outcome)
        assertEquals(listOf("X"), result.variables)
        assertEquals(4, result.solutions.size)
        assertEquals(listOf("peter", "mark", "micheal", "jj"), result.solutions.map { it.bindings["X"] })
        assertEquals(2.534e-5, result.executionSeconds)
        assertEquals("", result.output)
    }

    @Test
    fun `two variables group by the repeated name`() {
        // pi writes solutions back to back with no separator; a variable name
        // that has already been bound in this group starts the next solution
        val raw = "X=peter\nY=micheal\nX=peter\nY=jj\nX=mark\nY=micheal\n\n(execution time 0.00031209 seconds)\n"
        val result = Parser.parseQuery("?- half(X,Y).", raw)

        assertEquals(listOf("X", "Y"), result.variables)
        assertEquals(3, result.solutions.size)
        assertEquals(mapOf("X" to "peter", "Y" to "micheal"), result.solutions[0].bindings)
        assertEquals(mapOf("X" to "peter", "Y" to "jj"), result.solutions[1].bindings)
        assertEquals(mapOf("X" to "mark", "Y" to "micheal"), result.solutions[2].bindings)
    }

    @Test
    fun `a bare yes is a success with no bindings`() {
        val result = Parser.parseQuery("?- father(fred,peter).", "yes\n(execution time 0.00001010 seconds)\n")

        assertEquals(Outcome.SUCCESS, result.outcome)
        assertTrue(result.variables.isEmpty())
        assertEquals(1, result.solutions.size)
        assertTrue(result.solutions.single().bindings.isEmpty())
    }

    @Test
    fun `no is a failure`() {
        val result = Parser.parseQuery("?- father(fred,zoe).", "no\n(execution time 0.00000447 seconds)\n")

        assertEquals(Outcome.FAILURE, result.outcome)
        assertTrue(result.solutions.isEmpty())
        assertNull(result.error)
    }

    @Test
    fun `a parse error never reads as a solution`() {
        val raw = "error parsing query: expected ')' (line 1, character 17)\n"
        val result = Parser.parseQuery("?- father(fred,X", raw)

        assertEquals(Outcome.ERROR, result.outcome)
        assertTrue(result.error!!.startsWith("error parsing query:"))
        assertTrue(result.solutions.isEmpty())
    }

    @Test
    fun `written output is kept apart from the bindings`() {
        val raw = "hello\n42\nyes\n(execution time 0.00001374 seconds)\n"
        val result = Parser.parseQuery("?- write('hello'), nl, write(42), nl.", raw)

        assertEquals(Outcome.SUCCESS, result.outcome)
        assertEquals("hello\n42", result.output)
        assertTrue(result.variables.isEmpty())
    }

    @Test
    fun `written output before bindings`() {
        val raw = "moving\nX=2\n\n(execution time 0.00002164 seconds)\n"
        val result = Parser.parseQuery("?- write(moving), nl, X = 3 - 1.", raw)

        assertEquals("moving", result.output)
        assertEquals(listOf("X"), result.variables)
        assertEquals("2", result.solutions.single().bindings["X"])
    }

    // ------------------------------------------------------------ listings

    private val listing = """
        00001    father(fred,peter).
        00002    father(fred,mark).
        00009    different(X,Y) :- X!=Y.
        00010    half(X,Y) :- father(Z,X), father(Z,Y) ; father(A,X), father(B,Y).
        00011    zzz(1).
        00012    len([ ],0).
    """.trimIndent()

    @Test
    fun `a listing becomes numbered clauses with predicate indicators`() {
        val snapshot = Parser.parseListing(listing)

        assertEquals(6, snapshot.clauses.size)
        assertEquals(1, snapshot.clauses[0].index)
        assertEquals("father/2", snapshot.clauses[0].predicate)
        assertFalse(snapshot.clauses[0].isRule)
        assertEquals("father(fred,peter)", snapshot.clauses[0].text)
    }

    @Test
    fun `a rule is told from a fact by its neck`() {
        val snapshot = Parser.parseListing(listing)
        val rule = snapshot.clauses.single { it.index == 10 }

        assertTrue(rule.isRule)
        assertEquals("half/2", rule.predicate)
        assertEquals(2, rule.arity)
    }

    @Test
    fun `arity counts top level arguments only`() {
        val snapshot = Parser.parseListing("00001    p(f(a,b),[1,2,3],c).")
        assertEquals(3, snapshot.clauses.single().arity)
    }

    @Test
    fun `clauses group into predicates in listing order`() {
        val snapshot = Parser.parseListing(listing)

        assertEquals(
            listOf("father/2", "different/2", "half/2", "zzz/1", "len/2"),
            snapshot.predicates.map { it.indicator },
        )
        assertEquals(2, snapshot.predicates.first().clauses.size)
    }

    @Test
    fun `an empty database is not an error`() {
        val snapshot = Parser.parseListing("database empty\n")

        assertTrue(snapshot.clauses.isEmpty())
        assertNull(snapshot.error)
    }

    @Test
    fun `the revision changes only when the listing does`() {
        assertEquals(Parser.parseListing(listing).revision, Parser.parseListing(listing).revision)
        assertTrue(
            Parser.parseListing(listing).revision != Parser.parseListing("$listing\n00013    q(a).").revision,
        )
    }

    // ------------------------------------------------------------ input

    @Test
    fun `a program splits into one statement per line`() {
        val program = """
            % a list length rule
            len([],0).
            len([H|T],N) :-
                len(T,M),
                N = M + 1.
        """.trimIndent()

        assertEquals(
            listOf("len([],0).", "len([H|T],N) :- len(T,M), N = M + 1."),
            Parser.splitProgram(program),
        )
    }

    @Test
    fun `a dot inside a quoted string does not end a statement`() {
        assertEquals(
            listOf("says('one. two').", "b(2)."),
            Parser.splitProgram("says('one. two').\nb(2)."),
        )
    }

    @Test
    fun `comments are stripped before lines are joined`() {
        assertEquals(listOf("a(1)."), Parser.splitProgram("a(1).  % this would swallow the next line"))
    }

    @Test
    fun `goals and clauses are told apart and completed`() {
        assertTrue(Parser.isQuery("?- foo(X)."))
        assertFalse(Parser.isQuery("foo(bar)."))
        assertEquals("?- father(fred,X).", Parser.normaliseGoal("father(fred,X)"))
        assertEquals("?- father(fred,X).", Parser.normaliseGoal("?- father(fred,X)."))
        assertEquals("likes(pete,prolog).", Parser.normaliseClause("likes(pete,prolog)"))
    }

    // ------------------------------------------------------------ commands

    @Test
    fun `an assert that printed nothing succeeded`() {
        val result = Parser.parseCommand("zzz(1).", "\n")

        assertTrue(result.ok)
        assertEquals("ok", result.message)
    }

    @Test
    fun `a rejected clause is reported as a failure`() {
        val result = Parser.parseCommand("zzz(1", "error parsing statement(s): expected ')' (line 1, character 6)\n")

        assertFalse(result.ok)
        assertTrue(result.message.contains("expected ')'"))
    }
}
