#include <iostream>
#include <chrono> 

#include "Lexer.h"
#include "Parser.h"

using namespace diannex;

int main(int argc, char** argv)
{
    std::cout << "Parser testing" << std::endl;

    std::vector<Token> res = std::vector<Token>();
    auto start = std::chrono::high_resolution_clock::now();

    Lexer::LexString("//! Test string", res);
    std::unique_ptr<Node> parsed = Parser::ParseTokens(&res);

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    std::cout << "Took " << duration.count() << " milliseconds" << std::endl;
    std::cin.get();

	return 0;
}