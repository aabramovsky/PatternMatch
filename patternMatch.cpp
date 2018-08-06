// patternMatch.cpp : Defines the entry point for the console application.
//

#include <string>
#include <algorithm>
#include <vector>
#include <iostream>

#define ERROR_NODE_IDX -1
#define SUCCESS_NODE_IDX -2

#define PATH_SEPARATOR '/'
#define END_OF_LINE '\0'

bool symbolIs(const std::string& str, int pos, const char c)
{
  if (pos < 0 || pos > str.length())
    return false;

  return (str[pos] == c);
}

void standardizePathSeparators(std::string& s)
{
  std::replace(s.begin(), s.end(), '\\', '/'); // replace all 'x' to 'y'
}

void standardizePattern(std::string& pattern)
{
  int len = pattern.length();
  
  // If the pattern does not start with a path separator i.e. / or \, then the pattern is considered to start with /**
  if (!symbolIs(pattern, 0, PATH_SEPARATOR))
  {
    if (symbolIs(pattern, 0, '*') && symbolIs(pattern, 1, '*'))
    {
      pattern = "/" + pattern;
    }
    else
    {
      pattern = "/**/" + pattern;
    }
  }

  //If the pattern ends with / then ** is automatically appended.
  if (symbolIs(pattern, len - 1, PATH_SEPARATOR))
    pattern += "**";
}

enum SymbolType
{
  eOneSymbol,
  eEndOfLine,
  eAnyPathSegmentSymbol,
  eAnySymbol
};

struct InputSymbol
{
  SymbolType stype;
  char sym;

  InputSymbol() : stype(eAnySymbol), sym(0)
  {}

  InputSymbol(SymbolType t) : stype(t), sym(0)
  {}

  bool match(const char s)
  {
    bool bRet = false;

    switch (stype)
    {
    case eOneSymbol:
    {
      if (s == sym)
        bRet = true;
    }
    break;
    case eEndOfLine:
    {
      if (s == END_OF_LINE)
        bRet = true;
    }
    break;
    case eAnySymbol:
    {
      bRet = true;
    }
    break;
    case eAnyPathSegmentSymbol:
    {
      if (s != PATH_SEPARATOR && s != END_OF_LINE)
        bRet = true;
    }
    break;
    default:
      throw std::runtime_error("unexpected symbol type in InputSymbol");
    }

    return bRet;
  }
};

struct Edge
{
  InputSymbol symbol;
  int nextNodeId;
};

struct Node
{
  typedef std::vector<Edge> EdgesList;
  EdgesList edges;

  Node()
  {}

  static bool compare_edges_priority(const Edge& e1, const Edge& e2)
  {
    return (e1.symbol.stype < e2.symbol.stype);
  }

  void addEdge(const char c, int nodeId)
  {
    Edge e;
    e.symbol.stype = eOneSymbol;
    e.symbol.sym = c;
    e.nextNodeId = nodeId;

    edges.push_back(e);
    std::sort(edges.begin(), edges.end(), compare_edges_priority);
  }

  void addEdge(InputSymbol sym, int nodeId)
  {
    Edge e;
    e.symbol = sym;
    e.nextNodeId = nodeId;

    edges.push_back(e);
    std::sort(edges.begin(), edges.end(), compare_edges_priority);
  }

  int edgeCount() const
  {
    return edges.size();
  }

  bool selectEdge(int edgeIdx, const char sym, int& nextNodeId)
  {
    if (edgeIdx < 0 || edgeIdx >= edgeCount())
      return false;

    Edge& e = edges[edgeIdx];
    if (!e.symbol.match(sym))
      return false;

    nextNodeId = e.nextNodeId;
    return true;
  }
};


class SimpleStateMachine
{
public:
  SimpleStateMachine()
  {}
  ~SimpleStateMachine()
  {}

  void setPattern(std::string& pattern)
  {
    if (pattern.empty())
      return;

    Node aNode;
    nodesArray.push_back(aNode); // pushing initial node

    int previousStarNodeIdx = -1;

    for (int patternPos = 0; patternPos < pattern.length(); patternPos++)
    {
      const char sym = pattern[patternPos];
      
      switch (sym)
      {
        case PATH_SEPARATOR:
          {
            previousStarNodeIdx = -1;

            int currentNodeIdx = lastNodeIdx();
            int nextNodeIdx = createNewNode();

            addEdge(currentNodeIdx, PATH_SEPARATOR, nextNodeIdx);

            if (symbolIs(pattern, patternPos + 1, '*') && symbolIs(pattern, patternPos + 2, '*')) 
            {
              addEdge(nextNodeIdx, InputSymbol(eAnySymbol), nextNodeIdx);
              previousStarNodeIdx = nextNodeIdx;
              
              if (symbolIs(pattern, patternPos + 3, PATH_SEPARATOR)) { //skip '**' or '**/'
                patternPos += 3;
              }
              else {
                patternPos += 2;
              }
            }            
          }
          break;
        case '?':
          {
            int currentNodeIdx = lastNodeIdx();
            int nextNodeIdx = createNewNode();

            addEdge(currentNodeIdx, InputSymbol(eAnyPathSegmentSymbol), nextNodeIdx);

            previousStarNodeIdx = -1;
          }
          break;
        case '*':
          {
            int currentNodeIdx = lastNodeIdx();
            addEdge(currentNodeIdx, InputSymbol(eAnyPathSegmentSymbol), currentNodeIdx);
           
            previousStarNodeIdx = currentNodeIdx;
          }
          break;
        default:
          {
            int currentNodeIdx = lastNodeIdx();
            int nextNodeIdx = createNewNode();

            addEdge(currentNodeIdx, sym, nextNodeIdx);
            
            if (previousStarNodeIdx >= 0) // previous star node in this path segment
              addEdge(nextNodeIdx, InputSymbol(eAnyPathSegmentSymbol), previousStarNodeIdx);
          }
          break;
      }
    }

    int currentNodeIdx = lastNodeIdx();
    addEdge(currentNodeIdx, InputSymbol(eEndOfLine), SUCCESS_NODE_IDX);
  }

  bool match(std::string& path)
  {
    if (path.empty())
      return false;

    if (nodesArray.empty())
      throw std::runtime_error("no pattern was set for state machine");
    
    return tryMatchPatternPart(0, path, 0);
  }

  bool tryMatchPatternPart(int nodeIdx, std::string& path, int pathPos)
  {
    if (nodeIdx == SUCCESS_NODE_IDX)
      return true;

    if (nodeIdx == ERROR_NODE_IDX)
      return false;

    if (pathPos > path.length())
      return false;

    Node& currentNode = nodesArray[nodeIdx];
    const char sym = path[pathPos];
    
    int nextNodeId = ERROR_NODE_IDX;
    for (int edgeIdx = 0; edgeIdx < currentNode.edgeCount(); edgeIdx++)
    {
      if (currentNode.selectEdge(edgeIdx, sym, nextNodeId))
      {
        if (tryMatchPatternPart(nextNodeId, path, pathPos + 1))
        {
          return true;
        }
      }
    }

    return false;
  }

private:
  int createNewNode()
  {
    Node aNode;
    nodesArray.push_back(aNode);
    return (nodesArray.size() - 1);
  }

  int lastNodeIdx()
  {
    return nodesArray.size() - 1;
  }
  
  void addEdge(int fromNodeIdx, const char sym, int toNodeIdx)
  {
    nodesArray[fromNodeIdx].addEdge(sym, toNodeIdx);
  }

  void addEdge(int fromNodeIdx, InputSymbol input, int toNodeIdx)
  {
    nodesArray[fromNodeIdx].addEdge(input, toNodeIdx);
  }

private:
  std::vector<Node> nodesArray;
};

void usage()
{
  std::cout << "usage: patternMatch.exe [path] [pattern]";
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    usage();
    return 2;
  }

  std::string pattern = argv[2];
  std::string pathToMatch = argv[1];

  standardizePathSeparators(pattern);
  standardizePathSeparators(pathToMatch);

  standardizePattern(pattern);

  int ret = -1;
  try
  {
    SimpleStateMachine sm;
    sm.setPattern(pattern);
    bool bMatch = sm.match(pathToMatch);
    
    ret = bMatch ? 0 : 1;
  }
  catch (...)
  {
    ret = 2;
  }

  return ret;
}