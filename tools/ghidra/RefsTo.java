// Prints, for each address given, the places referring to it and, for a
// reference from data, its offset from the start of the run of pointers it
// sits in (a vtable slot).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;

public class RefsTo extends GhidraScript {
	@Override
	public void run() throws Exception {
		Memory mem = currentProgram.getMemory();
		for (String a : getScriptArgs()) {
			Address target = toAddr(a);
			for (Reference r : currentProgram.getReferenceManager().getReferencesTo(target)) {
				Address from = r.getFromAddress();
				// Walk back while the previous dword is also a pointer into code.
				Address start = from;
				while (true) {
					Address prev = start.subtract(4);
					int v;
					try { v = mem.getInt(prev); } catch (Exception e) { break; }
					Address p = toAddr(Integer.toUnsignedLong(v));
					if (getFunctionAt(p) == null) break;
					start = prev;
				}
				Symbol s = getSymbolAt(start);
				println(a + " <- " + from + " (" + r.getReferenceType() + ") table at " + start + " "
				        + (s == null ? "" : s.getName(true)) + " slot offset 0x" + Long.toHexString(from.subtract(start)));
			}
		}
	}
}
