/* Browser/WASM adapter for the PI Prolog engine. */

#include <emscripten/bind.h>

#include "database.h"
// engine.h expects Binding/BindingList/Set from node.h to be known first.
#include "node.h"
#include "engine.h"
#include "interpreter.h"
#include "system.h"

#include <string>

namespace
{
class StringWriter : public IOWriter
{
public:
	void Write(const std::string& text) override { output += text; }
	std::string output;
};

class WriterScope
{
public:
	explicit WriterScope(IOWriter* writer) : previous(System::GetWriter())
	{
		System::SetWriter(writer);
	}
	~WriterScope() { System::SetWriter(previous); }

private:
	IOWriter* previous;
};
}

class PiProlog
{
public:
	PiProlog()
	{
		System::GetTimer()->Init();
	}

	~PiProlog()
	{
		Engine::ResetStack();
		database.Clear();
	}

	std::string execute(const std::string& command)
	{
		StringWriter writer;
		WriterScope capture(&writer);
		ExecuteCommand(command,database);
		return writer.output;
	}

	std::string loadProgram(const std::string& source)
	{
		// ExecuteCommand treats non-query input as one or more statements, so
		// browser files do not need to enter Emscripten's virtual filesystem.
		return execute(source);
	}

	std::string reset()
	{
		Engine::ResetStack();
		database.Clear();
		return "program cleared\n";
	}

private:
	DataBase database;
};

EMSCRIPTEN_BINDINGS(pi_prolog)
{
	emscripten::class_<PiProlog>("PiProlog")
		.constructor<>()
		.function("execute",&PiProlog::execute)
		.function("loadProgram",&PiProlog::loadProgram)
		.function("reset",&PiProlog::reset);
}
