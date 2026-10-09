-- bootstrap lazy.nvim, LazyVim and your plugins
require("config.lazy")

-- using packer.nvim
use {'akinsho/bufferline.nvim', tag = "*", requires = 'nvim-tree/nvim-web-devicons'}

-- using lazy.nvim
{'akinsho/bufferline.nvim', version = "*", dependencies = 'nvim-tree/nvim-web-devicons'}
