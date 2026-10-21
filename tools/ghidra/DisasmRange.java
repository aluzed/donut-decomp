// Prints the instructions from <start> to <end>, disassembling them first when the
// analysis left them undefined (code reached only through a jump table, or not
// reached at all). Run on a read-only project, nothing is saved.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;

public class DisasmRange extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		Address start = toAddr(args[0]), end = toAddr(args[1]);
		disassemble(start);
		Address at = start;
		while (at.compareTo(end) < 0) {
			Instruction i = getInstructionAt(at);
			if (i == null) {
				disassemble(at);
				i = getInstructionAt(at);
			}
			if (i == null) {
				at = at.add(1);
				continue;
			}
			println(i.getAddress() + "  " + i.toString());
			at = i.getMaxAddress().add(1);
		}
	}
}
