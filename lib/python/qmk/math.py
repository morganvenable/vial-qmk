"""Parse arbitrary math equations in a safe way.

Gratefully copied from https://stackoverflow.com/a/9558001
"""
import ast
import operator as op

# supported operators
operators = {ast.Add: op.add, ast.Sub: op.sub, ast.Mult: op.mul, ast.Div: op.truediv, ast.Pow: op.pow, ast.BitXor: op.xor, ast.USub: op.neg}


def compute(expr):
    """Parse a mathematical expression and return the answer.

    >>> compute('2^6')
    4
    >>> compute('2**6')
    64
    >>> compute('1 + 2*3**(4^5) / (6 + -7)')
    -5.0
    """
    return _eval(ast.parse(expr, mode='eval').body)


def _eval(node):
    if isinstance(node, ast.Constant):  # Python 3.8+
        return node.value
    elif isinstance(node, ast.BinOp):
        return _operators[type(node.op)](_eval(node.left), _eval(node.right))
    elif isinstance(node, ast.UnaryOp):
        return _operators[type(node.op)](_eval(node.operand))
    else:
        raise TypeError(node)
