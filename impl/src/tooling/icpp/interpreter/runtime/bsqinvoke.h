//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include "../common.h"
#include "bsqop.h"

void jsonLoadBSQTypeDecl(json v);

void jsonLoadBSQLiteralDecl(json v, size_t& storageOffset, const BSQType*& gtype, std::string& lval);
void jsonLoadBSQConstantDecl(json v, size_t& storageOffset, BSQInvokeID& ikey, const BSQType*& gtype);

class BSQFunctionParameter 
{
public:
    const std::string name;
    const BSQType* ptype;

    BSQFunctionParameter(std::string name, const BSQType* ptype): name(name), ptype(ptype) {;}
    ~BSQFunctionParameter() {;}
};

class BSQInvokeDecl 
{
public:
    const std::string name;
    const BSQInvokeID ikey;

    const std::string srcFile;
    const SourceInfo sinfoStart;
    const SourceInfo sinfoEnd;
    
    const bool recursive;

    const std::vector<BSQFunctionParameter> params;
    const BSQType* resultType;

    const bool isUserCode;

    static std::vector<const BSQInvokeDecl*> g_invokes;

    BSQInvokeDecl(std::string name, BSQInvokeID ikey, std::string srcFile, SourceInfo sinfoStart, SourceInfo sinfoEnd, bool recursive, std::vector<BSQFunctionParameter> params, const BSQType* resultType, bool isUserCode)
    : name(name), ikey(ikey), srcFile(srcFile), sinfoStart(sinfoStart), sinfoEnd(sinfoEnd), recursive(recursive), params(params), resultType(resultType), isUserCode(isUserCode)
    {;}

    virtual ~BSQInvokeDecl() {;}

    virtual bool isPrimitive() const = 0;

    static void jsonLoad(json v);
};

class BSQInvokeBodyDecl : public BSQInvokeDecl 
{
public:
    const std::vector<InterpOp*> body;
    const uint32_t argmaskSize;

    const std::vector<ParameterInfo> paraminfo;
    const Argument resultArg;

    const size_t scalarstackBytes;
    const size_t mixedstackBytes;
    RefMask mixedMask;

    const uint32_t maskSlots;

    BSQInvokeBodyDecl(std::string name, BSQInvokeID ikey, std::string srcFile, SourceInfo sinfoStart, SourceInfo sinfoEnd, bool recursive, std::vector<BSQFunctionParameter> params, const BSQType* resultType, std::vector<ParameterInfo> paraminfo, Argument resultArg, size_t scalarstackBytes, size_t mixedstackBytes, RefMask mixedMask, uint32_t maskSlots, std::vector<InterpOp*> body, uint32_t argmaskSize, bool isusercode)
    : BSQInvokeDecl(name, ikey, srcFile, sinfoStart, sinfoEnd, recursive, params, resultType, isusercode), body(body), argmaskSize(argmaskSize), paraminfo(paraminfo), resultArg(resultArg), scalarstackBytes(scalarstackBytes), mixedstackBytes(mixedstackBytes), mixedMask(mixedMask), maskSlots(maskSlots)
    {;}

    virtual ~BSQInvokeBodyDecl()
    {
        std::for_each(this->body.begin(), this->body.end(), [](InterpOp* op) {
            delete(op);
        });
    }

    virtual bool isPrimitive() const override
    {
        return false;
    }

    static BSQInvokeBodyDecl* jsonLoad(json v);
};

class BSQPCode
{
public:
    const BSQInvokeID code;
    const std::vector<uint32_t> cargpos;

    BSQPCode(BSQInvokeID code, std::vector<uint32_t> cargpos): code(code), cargpos(cargpos) {;}
    ~BSQPCode() {;}
};

class BSQPCodeOperator
{
public:
    const BSQInvokeDecl* call;
    const std::vector<StorageLocationPtr> cargs;

    BSQPCodeOperator(const BSQInvokeDecl* call) : call(call), cargs() {;}
    ~BSQPCodeOperator() {;}
};

class BSQInvokePrimitiveDecl : public BSQInvokeDecl 
{
public:
    const BSQType* enclosingtype;
    const BSQPrimitiveImplTag implkey; 
    const std::string implkeyname;
    const std::map<std::string, const BSQType*> binds;
    const std::map<std::string, BSQPCode*> pcodes;

    BSQInvokePrimitiveDecl(std::string name, BSQInvokeID ikey, std::string srcFile, SourceInfo sinfoStart, SourceInfo sinfoEnd, bool recursive, std::vector<BSQFunctionParameter> params, const BSQType* resultType, size_t scalarstackBytes, size_t mixedstackBytes, RefMask mixedMask, uint32_t maskSlots, const BSQType* enclosingtype, BSQPrimitiveImplTag implkey, std::string implkeyname, std::map<std::string, const BSQType*> binds, std::map<std::string, BSQPCode*> pcodes)
    : BSQInvokeDecl(name, ikey, srcFile, sinfoStart, sinfoEnd, recursive, params, resultType, false), enclosingtype(enclosingtype), implkey(implkey), implkeyname(implkeyname), binds(binds), pcodes(pcodes)
    {;}

    virtual ~BSQInvokePrimitiveDecl() {;}

    virtual bool isPrimitive() const override
    {
        return true;
    }

    static BSQInvokePrimitiveDecl* jsonLoad(json v);
};

class LambdaEvalThunk
{
public:
    void* ctx;

    LambdaEvalThunk(void* ctx): ctx(ctx) {;}
    ~LambdaEvalThunk() {;}

    void invoke(const BSQInvokeBodyDecl* call, const std::vector<StorageLocationPtr>& args, StorageLocationPtr resultsl);
};
