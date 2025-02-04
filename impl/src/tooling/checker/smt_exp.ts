//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

import { SymbolicActionMode } from "../../compiler/mir_assembly";
import { MIRResolvedTypeKey } from "../../compiler/mir_ops";

type VerifierOptions = {
    INT_MIN: number,
    INT_MAX: number,
    SLEN_MAX: number,
    BLEN_MAX: number,

    ARRAY_MODE: "Seq" | "Array";
    CONTAINER_MAX: number,

    ActionMode: SymbolicActionMode
};

class SMTMaskConstruct {
    readonly maskname: string;
    readonly entries: SMTExp[] = [];

    constructor(maskname: string) {
        this.maskname = maskname;
    }

    emitSMT2(): string {
        return `($Mask_${this.entries.length}@cons ${this.entries.map((mv) => mv.emitSMT2(undefined)).join(" ")})`;
    }
}

class SMTTypeInfo {
    readonly smttypename: string;
    readonly smttypetag: string;
    readonly typeID: MIRResolvedTypeKey;
    
    constructor(smttypename: string, smttypetag: string, typeid: MIRResolvedTypeKey) {
        this.smttypename = smttypename;
        this.smttypetag = smttypetag;
        this.typeID = typeid;
    }

    isGeneralKeyType(): boolean {
        return this.smttypename === "BKey";
    }

    isGeneralTermType(): boolean {
        return this.smttypename === "BTerm";
    }
}

abstract class SMTExp {
    abstract emitSMT2(indent: string | undefined): string;

    abstract computeCallees(callees: Set<string>): void;
}

class SMTVar extends SMTExp {
    readonly vname: string;

    constructor(vname: string) {
        super();

        this.vname = vname;
    }

    emitSMT2(indent: string | undefined): string {
        return this.vname;
    }

    computeCallees(callees: Set<string>): void {
        //Nothing to do in many cases
    }
}

class SMTConst extends SMTExp {
    readonly cname: string;

    constructor(cname: string) {
        super();

        this.cname = cname;
    }

    emitSMT2(indent: string | undefined): string {
        return this.cname;
    }

    computeCallees(callees: Set<string>): void {
        //Nothing to do in many cases
    }
}

class SMTCallSimple extends SMTExp {
    readonly fname: string;
    readonly args: SMTExp[];

    constructor(fname: string, args: SMTExp[]) {
        super();

        this.fname = fname;
        this.args = args;
    }

    emitSMT2(indent: string | undefined): string {
        return this.args.length === 0 ? this.fname : `(${this.fname} ${this.args.map((arg) => arg.emitSMT2(undefined)).join(" ")})`;
    }

    computeCallees(callees: Set<string>): void {
        callees.add(this.fname);
        this.args.forEach((arg) => arg.computeCallees(callees));
    }

    static makeEq(lhs: SMTExp, rhs: SMTExp): SMTExp {
        return new SMTCallSimple("=", [lhs, rhs]);
    }

    static makeNotEq(lhs: SMTExp, rhs: SMTExp): SMTExp {
        return new SMTCallSimple("not", [new SMTCallSimple("=", [lhs, rhs])]);
    }

    static makeBinOp(op: string, lhs: SMTExp, rhs: SMTExp): SMTExp {
        return new SMTCallSimple(op, [lhs, rhs]);
    }

    static makeIsTypeOp(smtname: string, exp: SMTExp): SMTExp {
        return new SMTCallSimple(`(_ is ${smtname})`, [exp]);
    }

    static makeNot(exp: SMTExp): SMTExp {
        return new SMTCallSimple("not", [exp]);
    }

    static makeAndOf(...exps: SMTExp[]): SMTExp {
        if(exps.length === 1) {
            return exps[0];
        }
        else {
            return new SMTCallSimple("and", exps);
        }
    }

    static makeOrOf(...exps: SMTExp[]): SMTExp {
        if(exps.length === 1) {
            return exps[0];
        }
        else {
            return new SMTCallSimple("or", exps);
        }
    }
}

class SMTCallGeneral extends SMTExp {
    readonly fname: string;
    readonly args: SMTExp[];

    constructor(fname: string, args: SMTExp[]) {
        super();

        this.fname = fname;
        this.args = args;
    }

    emitSMT2(indent: string | undefined): string {
        return this.args.length === 0 ? this.fname : `(${this.fname} ${this.args.map((arg) => arg.emitSMT2(undefined)).join(" ")})`;
    }

    computeCallees(callees: Set<string>): void {
        callees.add(this.fname);
        this.args.forEach((arg) => arg.computeCallees(callees));
    }
}

class SMTCallGeneralWOptMask extends SMTExp {
    readonly fname: string;
    readonly args: SMTExp[];
    readonly mask: SMTMaskConstruct;

    constructor(fname: string, args: SMTExp[], mask: SMTMaskConstruct) {
        super();

        this.fname = fname;
        this.args = args;
        this.mask = mask;
    }

    emitSMT2(indent: string | undefined): string {
        return this.args.length === 0 ? `(${this.fname} ${this.mask.emitSMT2()})` : `(${this.fname} ${this.args.map((arg) => arg.emitSMT2(undefined)).join(" ")} ${this.mask.emitSMT2()})`;
    }

    computeCallees(callees: Set<string>): void {
        callees.add(this.fname);
        this.args.forEach((arg) => arg.computeCallees(callees));

        this.mask.entries.forEach((mentry) => mentry.computeCallees(callees));
    }
}

class SMTCallGeneralWPassThroughMask extends SMTExp {
    readonly fname: string;
    readonly args: SMTExp[];
    readonly mask: string;

    constructor(fname: string, args: SMTExp[], mask: string) {
        super();

        this.fname = fname;
        this.args = args;
        this.mask = mask;
    }

    emitSMT2(indent: string | undefined): string {
        return this.args.length === 0 ? `(${this.fname} ${this.mask})` : `(${this.fname} ${this.args.map((arg) => arg.emitSMT2(undefined)).join(" ")} ${this.mask})`;
    }

    computeCallees(callees: Set<string>): void {
        callees.add(this.fname);
        this.args.forEach((arg) => arg.computeCallees(callees));
    }
}

class SMTLet extends SMTExp {
    readonly vname: string;
    readonly value: SMTExp;
    readonly inexp: SMTExp;

    constructor(vname: string, value: SMTExp, inexp: SMTExp) {
        super();

        this.vname = vname;
        this.value = value;
        this.inexp = inexp;
    }

    emitSMT2(indent: string | undefined): string {
        if (indent === undefined) {
            return `(let ((${this.vname} ${this.value.emitSMT2(undefined)})) ${this.inexp.emitSMT2(undefined)})`;
        }
        else {
            return `(let ((${this.vname} ${this.value.emitSMT2(undefined)}))\n${indent + "  "}${this.inexp.emitSMT2(indent + "  ")}\n${indent})`;
        }
    }

    computeCallees(callees: Set<string>): void {
        this.value.computeCallees(callees);
        this.inexp.computeCallees(callees);
    }
}

class SMTLetMulti extends SMTExp {
    readonly assigns: { vname: string, value: SMTExp }[];
    readonly inexp: SMTExp

    constructor(assigns: { vname: string, value: SMTExp }[], inexp: SMTExp) {
        super();

        this.assigns = assigns;
        this.inexp = inexp;
    }

    emitSMT2(indent: string | undefined): string {
        const binds = this.assigns.map((asgn) => `(${asgn.vname} ${asgn.value.emitSMT2(undefined)})`);

        if (indent === undefined) {
            return `(let (${binds.join(" ")}) ${this.inexp.emitSMT2(undefined)})`;
        }
        else {
            return `(let (${binds.join(" ")})\n${indent + "  "}${this.inexp.emitSMT2(indent + "  ")}\n${indent})`;
        }
    }

    computeCallees(callees: Set<string>): void {
        this.assigns.forEach((asgn) => {
            asgn.value.computeCallees(callees);
        });
        this.inexp.computeCallees(callees);
    }
}

class SMTIf extends SMTExp {
    readonly cond: SMTExp;
    readonly tval: SMTExp;
    readonly fval: SMTExp;

    constructor(cond: SMTExp, tval: SMTExp, fval: SMTExp) {
        super();

        this.cond = cond;
        this.tval = tval;
        this.fval = fval;
    }

    emitSMT2(indent: string | undefined): string {
        if (indent === undefined) {
            return `(ite ${this.cond.emitSMT2(undefined)} ${this.tval.emitSMT2(undefined)} ${this.fval.emitSMT2(undefined)})`;
        }
        else {
            return `(ite ${this.cond.emitSMT2(undefined)}\n${indent + "  "}${this.tval.emitSMT2(indent + "  ")}\n${indent + "  "}${this.fval.emitSMT2(indent + "  ")}\n${indent})`;
        }
    }

    computeCallees(callees: Set<string>): void {
        this.cond.computeCallees(callees);
        this.tval.computeCallees(callees);
        this.fval.computeCallees(callees);
    }
}

class SMTCond extends SMTExp {
    readonly opts: {test: SMTExp, result: SMTExp}[];
    readonly orelse: SMTExp;

    constructor(opts: {test: SMTExp, result: SMTExp}[], orelse: SMTExp) {
        super();

        this.opts = opts;
        this.orelse = orelse;
    }

    emitSMT2(indent: string | undefined): string {
        if (indent === undefined) {
            let iopts: string = this.orelse.emitSMT2(undefined);
            for(let i = this.opts.length - 1; i >= 0; --i) {
                iopts = `(ite ${this.opts[i].test.emitSMT2(undefined)} ${this.opts[i].result.emitSMT2(undefined)} ${iopts})`
            }
            return iopts;
        }
        else {
            let iopts: string = this.orelse.emitSMT2(undefined);
            for(let i = this.opts.length - 1; i >= 0; --i) {
                iopts = `(ite ${this.opts[i].test.emitSMT2(undefined)}\n${indent + "  "}${this.opts[i].result.emitSMT2(indent + "  ")}\n${indent + "  "}${iopts}\n${indent})`
            }
            return iopts;
        }
    }

    computeCallees(callees: Set<string>): void {
        this.opts.forEach((opt) => {
            opt.test.computeCallees(callees);
            opt.result.computeCallees(callees);
        });
        this.orelse.computeCallees(callees);
    }
}

export {
    VerifierOptions,
    SMTMaskConstruct,
    SMTTypeInfo, SMTExp, SMTVar, SMTConst, 
    SMTCallSimple, SMTCallGeneral, SMTCallGeneralWOptMask, SMTCallGeneralWPassThroughMask,
    SMTLet, SMTLetMulti, SMTIf, SMTCond
};
