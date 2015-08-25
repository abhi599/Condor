#include "check.h"
#include "cobra/mem/isolate.h"
#include "cobra/flags.h"

namespace Cobra{
namespace internal{

	Check::Check(){
		scope = NULL;
		trace = TRACE_CHECK; // debug
		col = 0;
		row = 0;
		printIndent = 0;
		file = NULL;
	}

	Check::~Check(){
		//delete file;
		//delete scope;
	}

	void Check::CheckFile(ASTFile* f){
		file = f;
		OpenBaseScope();
		CountItemsInScope();
		ScanScope();
		SetScope();
	}

	std::string Check::GetTokenString(TOKEN tok){
		Token* token = new Token(tok);
		std::string result = token->String();
		delete token;
		return result;
	}

	void Check::Trace(std::string msg1, std::string msg2){
		std::string ident = "";
		for (int i = 0; i < printIndent; i++){
			ident += "  ";
			if (i == printIndent - 1){
				ident += "- ";
			}
		}
		printf("%s%s - %s\n", ident.c_str(), msg1.c_str(), msg2.c_str());
	}

	ASTNode* Check::GetObjectInScopeByString(std::string name, Scope* sc){
		if (sc == NULL){
			return NULL;
		}
		ASTNode* node = sc->Lookup(name);
		if (node == NULL){
			node = GetObjectInScopeByString(name, sc->outer);
		}
		return node;
	}

	ASTNode* Check::GetObjectInScope(ASTIdent* ident, Scope* sc){
		if (sc == NULL || ident == NULL){
			return NULL;
		}
		ASTNode* node = sc->Lookup(ident->name);
		if (node == NULL && sc->outer != NULL){
			node = GetObjectInScope(ident, sc->outer);
		}
		return node;
	}

	void Check::ValidateFuncCall(ASTFuncCallExpr* call){
		if (trace) Trace("Calling func", call->name);
		ASTFunc* func = (ASTFunc*) GetObjectInScopeByString(call->name, scope);
		if (func == NULL) throw Error::UNDEFINED_FUNC;
		call->func = func;
		printIndent++;
		for (int i = 0; i < call->params.size(); i++){
			ValidateStmt(call->params[i]);
		}
		printIndent--;
	}

	void Check::ValidateIdent(ASTIdent* ident){
		ASTNode* ptr = GetObjectInScope(ident, scope);
		if (ptr == NULL) throw Error::UNDEFINED_VARIABLE;
		else{
			ident->value = ptr;
			if (ident->inc){
				if (ident->post){
					if (trace) Trace("inc", ident->name + "++");
				}
				else if (ident->pre){
					if (trace) Trace("inc", "++" + ident->name);
				}
			}
			else if (ident->dec){
				if (ident->post){
					if (trace) Trace("inc", ident->name + "--");
				}
				else if (ident->pre){
					if (trace) Trace("inc", "--" + ident->name);
				}
			}
			else{
				if (trace) Trace(ptr->name, "is a " + GetTokenString(ptr->type));
			}
		}
	}

	void Check::ValidateBinaryStmt(ASTBinaryExpr* binary){
		if (binary == NULL){
			if (trace) Trace("Binary", "is NULL");
		}
		else if (binary->op == NULL){

		}
		else{
			if (binary->Right != NULL) ValidateStmt(binary->Right);
			if (trace) Trace("Operator found", binary->op->String());
			if (binary->Left != NULL) ValidateStmt(binary->Left);
		}
	}

	void Check::ValidateLiterary(ASTLiterary* lit){
		if (trace) Trace("Lit value", lit->value);
	}

	void Check::ValidateUnaryStmt(ASTUnaryExpr* unary){
		if (unary == NULL){
			if (trace) Trace("Unary", "is NULL");
		}
		else if (unary->op == NULL){
			
		}
		else{
			if (trace) Trace("Operator found", unary->op->String());
			ValidateStmt(unary->value);
		}
	}

	void Check::ValidateStmt(ASTExpr* expr){
		if (expr == NULL) return;
		int type = (int) expr->type;
		switch (type){
			case UNARY: {
				ValidateUnaryStmt((ASTUnaryExpr*) expr);
				break;
			}
			case BINARY: {
				ValidateBinaryStmt((ASTBinaryExpr*) expr);
				break;
			}
			case LITERARY: {
				ValidateLiterary((ASTLiterary*) expr);
				break;
			}
			case IDENT: {
				ValidateIdent((ASTIdent*) expr);
				break;
			}
			case FUNC_CALL: {
				ValidateFuncCall((ASTFuncCallExpr*) expr);
				break;
			}
			case ASTCAST_EXPR: {
				ValidateCast((ASTCastExpr*) expr);
				break;
			}
			case OBJECT_MEMBER_CHAIN: {
				if (!ValidateObjectChainMember((ASTObjectMemberChainExpr*) expr)){
					throw Error::UNIDENTIFIED_OBJECT_MEMBER;
				}
				break;
			}
			default: {
				Trace("Didn't process, but found", GetTokenString(expr->type));
			}
		}
	}

	/**
	 * TODO: 
	 * 		FuncCall and func should have the same type
	 */
	bool Check::ValidateObjectChainMember(ASTObjectMemberChainExpr* member){
		std::map<std::string, ASTInclude*>::const_iterator it = file->includes.find(member->object->name);
		if (it != file->includes.end()){
			Script* script = isolate->GetContext()->GetScriptByString(isolate, it->second->name);
			ASTNode* exported = script->GetExportedObject(member->member->name);
			if (member->member->type == FUNC_CALL && exported->type == FUNC){
				ASTFuncCallExpr* funcCall = (ASTFuncCallExpr*) member->member;
				ASTFunc* func = (ASTFunc*) exported;

				if (funcCall->params.size() != func->args.size()){
					throw Error::INVALID_FUNC_CALL;
				}
				return true;
			}
			else if (member->member->type == IDENT && exported->type == IDENT){
				return true;
			}
		}
		return false;
	}

	void Check::ValidateCast(ASTCastExpr* cast){
		ASTExpr* to = cast->to;
		if (to == NULL || to->type != LITERARY){
			throw Error::UNKNOWN_CAST_TYPE;
		}
		else{
			ASTLiterary* lit = (ASTLiterary*) to;
			cast->castType = lit->kind;
		}
		ValidateStmt(cast->value);
	}
	
	void Check::ValidateVar(ASTNode* node){
		if (node->type == VAR){
			ASTVar* var = (ASTVar*) node;
			if (var->varClass != NULL && trace) Trace("\nVar type", var->varClass->name);
			else if (trace) Trace("\nValidating " + GetTokenString(var->type), node->name);
			ASTNode* stmt = var->stmt;
			ValidateStmt((ASTExpr*) stmt);
		}
	}

	void Check::ValidateFor(ASTNode* node){
		if (trace) Trace("Validating", "For loop");
		ASTFor* forStmt = (ASTFor*) node;
		OpenScope(forStmt->block->scope);
		ValidateVar(forStmt->var);
		scope->Insert(forStmt->var);
		ValidateStmt(forStmt->conditions);
		ValidateStmt(forStmt->iterator);
		ScanScope();
		CloseScope();
	}

	void Check::CheckScopeLevelNode(ASTNode* node){
		if (node->scan){
			int type = (int) node->type;
			switch (type){
				case FUNC_CALL: {
					ValidateFuncCall((ASTFuncCallExpr*) node);
					break;
				}
				case FUNC: {
					ASTNode* expr = scope->Lookup(node->name);
					ValidateFunc((ASTFunc*) expr);
					break;
				}
				case FOR: {
					ValidateFor(node);
					break;
				}
				case VAR: {
					ValidateVar(node);
					break;
				}
				default: {
					if (trace) Trace("Working on type...", GetTokenString(node->type));
				}
			}
		}
	}

	void Check::ScanScope(){
		if (scope->Size() > 0){
			for (int i = 0; i < scope->Size(); i++){
				CheckScopeLevelNode(scope->Get(i));
			}
		}
	}

	void Check::ValidateFuncArgs(ASTFunc* func){
		if (func->ordered.size() > 0){
			for (int i = 0; i < func->ordered.size(); i++){
				ASTNode* node = func->ordered[i];
				if (node->name.length() < 1) throw Error::EXPECTED_ARG_NAME;
				if (trace) Trace("arg[" + std::to_string(i) + "]", node->name);
				node->scan = false;
				func->body->scope->Insert(node); // add the arg to the func scope
			}
		}
	}

	void Check::ValidateFunc(ASTFunc* func){
		if (func == NULL) return;
		if (trace) Trace("\nValidating func", func->name + "\n------------------------------");
		if (trace) Trace(func->name + "() total args: ", std::to_string(func->ordered.size()));
		ValidateFuncArgs(func);
		
		OpenScope(func->body->scope);
		CountItemsInScope();
		if (trace) Trace("\nFunc Body", "\n------------------");
		ScanScope();
		CloseScope();
	}

	bool Check::HasMain(){
		bool hasMain = scope->Lookup("main") != NULL;
		if (trace && hasMain) Trace("Base Scope", "Has main");
		else if (trace) Trace("Base Scope", "Does not have main");
		return hasMain;
	}

	void Check::CountItemsInScope(){
		if (trace) Trace("Total items in scope", std::to_string(scope->Size()));
	}

	void Check::CloseScope(){
		scope = scope->outer;
	}

	void Check::OpenScope(Scope* s){
		scope = s;
	}

	void Check::SetScope(){
		file->scope = scope;
	}

	void Check::OpenBaseScope(){
		if (file->scope != NULL){
			if (trace) Trace("Base Scope", "Is Opened");
			scope = file->scope;
		}
		else{
			if (trace) Trace("Base Scope", "Is Null");
		}
	}
} // namespace internal
} // namespace Cobra