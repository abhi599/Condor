#ifndef INTERNAL_H_
#define INTERNAL_H_

#include "cobra/ast/node.h"
#include "cobra/token/token.h"
#include "cobra/global.h"

#include <string>
#include <iostream>

namespace Cobra {
namespace internal{

	class ASTNode;
	typedef ASTNode* (*InternalFunctionCallback)(Isolate* iso, ASTNode* node);

	class Internal
	{
	public:
		static ASTNode* PrintF(Isolate* iso, ASTNode* lit);
		static ASTNode* ReadLine(Isolate* iso, ASTNode* lit);
		static ASTNode* CallInternal(Isolate* iso, InternalFunctionCallback call, ASTNode* node){return call(iso, node);}
	};

} // namespace internal
}

#endif // INTERNAL_H_
