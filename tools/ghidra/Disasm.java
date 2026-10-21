// Prints the instructions of the function containing each address given.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class Disasm extends GhidraScript {
	@Override
	public void run() throws Exception {
		for (String a : getScriptArgs()) {
			Function f = getFunctionContaining(toAddr(a));
			println("==== " + f.getName(true) + " at " + f.getEntryPoint());
			for (Instruction i : currentProgram.getListing().getInstructions(f.getBody(), true))
				println(i.getAddress() + "  " + i.toString());
		}
	}
}
