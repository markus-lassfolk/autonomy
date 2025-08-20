# autonomy Documentation Website

This directory contains the documentation website for the autonomy project, built with Jekyll and automatically deployed via GitHub Actions.

## 🚀 Quick Start

### Local Development

1. **Install Ruby and Jekyll**:
   ```bash
   # Install Ruby (if not already installed)
   # On macOS: brew install ruby
   # On Ubuntu: sudo apt install ruby ruby-dev
   
   # Install Jekyll and dependencies
   cd docs
   bundle install
   ```

2. **Run the development server**:
   ```bash
   bundle exec jekyll serve
   ```

3. **View the site**: Open http://localhost:4000 in your browser

### Building for Production

```bash
# Build the site
bundle exec jekyll build

# Build with production settings
JEKYLL_ENV=production bundle exec jekyll build
```

## 📁 Directory Structure

```
docs/
├── _config.yml          # Jekyll configuration
├── Gemfile              # Ruby dependencies
├── index.md             # Documentation homepage
├── README.md            # This file
├── QUICK_START.md       # Installation and setup guide
├── DEVELOPMENT.md       # Development guide
├── CONFIGURATION.md     # Configuration reference
├── API_REFERENCE.md     # API documentation
├── DEPLOYMENT.md        # Deployment guide
├── TROUBLESHOOTING.md   # Troubleshooting guide
└── [other .md files]    # Additional documentation
```

## 🔧 Configuration

### Jekyll Configuration (`_config.yml`)

The main configuration file controls:
- Site metadata (title, description, URL)
- Build settings (markdown processor, syntax highlighting)
- Navigation structure
- Sidebar organization
- Plugin settings

### Navigation Structure

The navigation is defined in `_config.yml` under the `navigation` and `sidebar` sections:

```yaml
navigation:
  - title: Home
    url: /
  - title: Quick Start
    url: /docs/quick-start.html

sidebar:
  - title: Getting Started
    children:
      - title: Quick Start
        url: /docs/quick-start.html
```

## 📝 Adding New Documentation

### Creating New Pages

1. **Create a new Markdown file** in the `docs/` directory
2. **Add front matter** at the top of the file:
   ```markdown
   ---
   layout: default
   title: Your Page Title
   description: Brief description of the page
   ---
   
   # Your Content Here
   ```

3. **Update navigation** in `_config.yml` if needed

### Documentation Standards

- Use clear, descriptive titles
- Include front matter with title and description
- Use proper Markdown formatting
- Include code examples where appropriate
- Link to related documentation
- Keep content focused and well-organized

## 🚀 Automatic Deployment

The documentation website is automatically built and deployed via GitHub Actions:

### Workflow Triggers

- **Push to main branch**: Deploys to production
- **Pull requests**: Creates preview deployments
- **Path changes**: Only triggers on documentation changes

### Deployment Process

1. **Build**: Jekyll builds the site from Markdown files
2. **Test**: Validates the build output
3. **Deploy**: Publishes to GitHub Pages
4. **Notify**: Comments on PRs with preview URLs

### Preview URLs

For pull requests, preview URLs are automatically generated:
```
https://your-org.github.io/autonomy/preview/PR_NUMBER/
```

## 🎨 Customization

### Themes and Styling

The site uses a custom Jekyll theme. To customize:

1. **Modify CSS**: Edit theme files in `_layouts/` and `_includes/`
2. **Add custom layouts**: Create new layout files in `_layouts/`
3. **Include custom scripts**: Add JavaScript files to `assets/`

### Plugins

Current plugins:
- `jekyll-seo-tag`: SEO optimization
- `jekyll-sitemap`: Automatic sitemap generation
- `jekyll-feed`: RSS feed generation

## 🔍 SEO and Performance

### SEO Features

- Automatic meta tags
- Open Graph support
- Twitter Card support
- Sitemap generation
- Structured data markup

### Performance Optimizations

- Minified CSS and JavaScript
- Optimized images
- Lazy loading
- CDN integration (if configured)

## 🐛 Troubleshooting

### Common Issues

1. **Build failures**: Check Ruby version and dependencies
2. **Missing pages**: Verify front matter and file paths
3. **Broken links**: Use relative paths and check navigation
4. **Styling issues**: Check CSS and layout files

### Debug Commands

```bash
# Check Jekyll configuration
bundle exec jekyll doctor

# Build with verbose output
bundle exec jekyll build --verbose

# Serve with draft posts
bundle exec jekyll serve --drafts
```

## 📚 Resources

- [Jekyll Documentation](https://jekyllrb.com/docs/)
- [GitHub Pages](https://pages.github.com/)
- [Markdown Guide](https://www.markdownguide.org/)
- [Liquid Template Language](https://shopify.github.io/liquid/)

## 🤝 Contributing

When contributing to documentation:

1. **Follow the existing structure** and formatting
2. **Test locally** before submitting changes
3. **Update navigation** if adding new sections
4. **Use descriptive commit messages**
5. **Include screenshots** for UI changes

## 📄 License

This documentation is part of the autonomy project and follows the same license terms.
