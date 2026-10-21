// Prints every instruction using one of the scalars given (hex), with its function.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;

public class FindScalar extends GhidraScript {
	@Override
	public void run() throws Exception {
		java.util.Set<Long> wanted = new java.util.HashSet<>();
		for (String a : getScriptArgs()) wanted.add(Long.parseUnsignedLong(a, 16));
		for (Instruction i : currentProgram.getListing().getInstructions(true)) {
			for (int op = 0; op < i.getNumOperands(); ++op)
				for (Object o : i.getOpObjects(op))
					if (o instanceof Scalar && wanted.contains(((Scalar) o).getUnsignedValue())) {
						Function f = getFunctionContaining(i.getAddress());
						println(i.getAddress() + " " + i + " in " + (f == null ? "?" : f.getEntryPoint() + " " + f.getName(true)));
					}
		}
	}
}
