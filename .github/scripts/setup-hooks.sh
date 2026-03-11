#!/usr/bin/env bash
#
# Setup script: Installs git hooks and initializes development environment
# Run with: bash .github/scripts/setup-hooks.sh
#

set -e

REPO_ROOT="$(git rev-parse --show-toplevel)"
HOOKS_SRC="$REPO_ROOT/.github/hooks"
HOOKS_DST="$REPO_ROOT/.git/hooks"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

echo -e "${YELLOW}═══════════════════════════════════════════${NC}"
echo -e "${YELLOW}Pudim-Luarix Development Environment Setup${NC}"
echo -e "${YELLOW}═══════════════════════════════════════════${NC}"
echo ""

# Check if .git/hooks exists
if [ ! -d "$HOOKS_DST" ]; then
    echo -e "${RED}✗ Not a git repository or .git/hooks missing${NC}"
    exit 1
fi

# Install pre-commit hook
echo -e "${YELLOW}[1/3] Installing pre-commit hook...${NC}"
if [ -f "$HOOKS_SRC/pre-commit" ]; then
    cp "$HOOKS_SRC/pre-commit" "$HOOKS_DST/pre-commit"
    chmod +x "$HOOKS_DST/pre-commit"
    echo -e "${GREEN}✓ pre-commit hook installed${NC}"
else
    echo -e "${RED}✗ pre-commit hook source not found${NC}"
    exit 1
fi

# Display hook info
echo ""
echo -e "${YELLOW}[2/3] Installed hooks:${NC}"
ls -lh "$HOOKS_DST" | grep -E 'pre-commit|post-commit' || echo "  (none yet configured)"

# Create symlink to instructions (optional)
echo ""
echo -e "${YELLOW}[3/3] Kernel safety standards:${NC}"
if [ -f "$REPO_ROOT/.github/instructions/buffer-overflow-safety.instructions.md" ]; then
    echo -e "${GREEN}✓ Buffer/overflow safety instruction available${NC}"
    echo "  Location: .github/instructions/buffer-overflow-safety.instructions.md"
fi

if [ -f "$REPO_ROOT/.github/prompts/c-to-lua-bindings.prompt.md" ]; then
    echo -e "${GREEN}✓ C→Lua bindings prompt available${NC}"
    echo "  Location: .github/prompts/c-to-lua-bindings.prompt.md"
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ Setup complete!${NC}"
echo -e "${GREEN}═══════════════════════════════════════════${NC}"
echo ""
echo "Next steps:"
echo "  1. Run: make clean && make"
echo "  2. Test hooks: git status"
echo "  3. Try committing: git commit --allow-empty -m 'test'"
echo ""
echo "For more info:"
echo "  - Binding generation: cat .github/prompts/c-to-lua-bindings.prompt.md"
echo "  - Memory safety: cat .github/instructions/buffer-overflow-safety.instructions.md"
echo "  - Docs location: cat .github/instructions/docs-location.instructions.md"
echo ""
