import { useState, useEffect } from 'react';
import clsx from 'clsx';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import styles from './index.module.css';

function useLatestVersion() {
  const [version, setVersion] = useState('v0.1.0');

  useEffect(() => {
    fetch('https://api.github.com/repos/GalaxyHaze/Zith/releases/latest')
      .then(res => res.json())
      .then(data => {
        if (data.tag_name) setVersion(data.tag_name);
      })
      .catch(() => {});
  }, []);

  return version;
}

export default function Home() {
  const {siteConfig} = useDocusaurusContext();
  const version = useLatestVersion();
  
  return (
    <Layout title="Home" description={siteConfig.tagline}>
      <main className={styles.main}>
        
        {/* Hero Section */}
        <section className={styles.hero}>
          <span className={styles.heroBadge}>{version}</span>
          <Heading as="h1" className={styles.heroTitle}>
            {siteConfig.title}
          </Heading>
          <p className={styles.heroDescription}>
            {siteConfig.tagline}
          </p>
          <div className={styles.heroCtas}>
            <Link className="button button--primary button--lg" to="/docs/intro/overview">
              Get Started
            </Link>
            <Link className="button button--secondary button--lg" to="/docs/language/overview">
              Language Guide
            </Link>
          </div>
        </section>

        {/* Code Preview Section */}
        <section className={styles.codePreview}>
          <div className={styles.codeWindow}>
            <div className={styles.codeWindowHeader}>
              <span className={styles.dot}></span>
              <span className={styles.dot}></span>
              <span className={styles.dot}></span>
              <span className={styles.codeFileName}>hello.zith</span>
            </div>
            <pre className={styles.codeContent}>
              <code>{`from std/io/console;
    fn main() {
    println("Hello, Zith");
}`}</code>
            </pre>
          </div>
        </section>

        {/* Features Grid */}
        <section className={styles.features}>
          <Heading as="h2">Why Zith?</Heading>
          <p className={styles.sectionSubtitle}>
            A language designed for clarity, safety, and performance
          </p>
          
          <div className={styles.featuresGrid}>
            <div className={styles.featureCard}>
              <div className={styles.featureIcon}>🎯</div>
              <h3>Clear & Expressive</h3>
              <p>
                Syntax that reads like English. No cryptic symbols or surprising behaviors.
                What you see is what you get.
              </p>
            </div>
            
            <div className={styles.featureCard}>
              <div className={styles.featureIcon}>🛡️</div>
              <h3>Memory Safe</h3>
              <p>
                Ownership system prevents memory bugs at compile time. No null pointer exceptions,
                no data races, no manual memory management.
              </p>
            </div>
            
            <div className={styles.featureCard}>
              <div className={styles.featureIcon}>⚡</div>
              <h3>Systems Performance</h3>
              <p>
                Compiles to native code with zero-cost abstractions. Control low-level details
                when you need to, but stay productive.
              </p>
            </div>
            
            <div className={styles.featureCard}>
              <div className={styles.featureIcon}>🔄</div>
              <h3>Modern Developer UX</h3>
              <p>
                Built-in formatting, testing, package management, and documentation generation.
                Tools that work for you.
              </p>
            </div>
          </div>
        </section>

        {/* Quick Links */}
        <section className={styles.quickLinks}>
          <Heading as="h2">Quick Links</Heading>
          
          <div className={styles.quickLinksGrid}>
            <Link to="/docs/intro/installation" className={styles.quickLinkCard}>
              <h4>Installation</h4>
              <p>Get Zith running on your system in minutes</p>
            </Link>
            
            <Link to="/docs/quickstart/quickstart" className={styles.quickLinkCard}>
              <h4>Quick Start</h4>
              <p>Write your first Zith program in 5 minutes</p>
            </Link>
            
            <Link to="/docs/cli/overview" className={styles.quickLinkCard}>
              <h4>CLI Reference</h4>
              <p>Build, run, test, and format your code</p>
            </Link>
            
            <Link to="/docs/faq/overview" className={styles.quickLinkCard}>
              <h4>FAQ</h4>
              <p>Common questions answered</p>
            </Link>
          </div>
        </section>

        {/* Projects Section - For future multi-project support */}
        <section className={styles.projects}>
          <Heading as="h2">Ecosystem</Heading>
          <p className={styles.sectionSubtitle}>
            Projects and tools built with Zith
          </p>
          
          <div className={styles.projectsGrid}>
            <div className={styles.projectCard}>
              <h4>Zith Core</h4>
              <p>The official compiler and standard library</p>
              <span className={styles.projectBadge}>Core</span>
            </div>
            
            <div className={styles.projectCard}>
              <h4>Zith LSP</h4>
              <p>Language Server Protocol implementation</p>
              <span className={styles.projectBadge}>Tools</span>
            </div>
            
            <div className={styles.projectCard}>
              <h4>Add Your Project</h4>
              <p>Building something with Zith? Share it with the community!</p>
              <Link to="/docs/community/overview" className={styles.projectLink}>
                Join Community →
              </Link>
            </div>
          </div>
        </section>

        {/* Footer CTA */}
        <section className={styles.footerCta}>
          <Heading as="h2">Ready to Try Zith?</Heading>
          <p>Join the community and start building today</p>
          <Link to="/docs/community/overview" className="button button--secondary button--lg" style={{ marginTop: '16px' }}>
              Join Community
          </Link>
          <div className={styles.footerCtaButtons}>
            <Link className="button button--primary button--lg" to="/docs/intro/overview">
              Get Started
            </Link>
            <Link className="button button--lg buttonOutline" href="https://github.com/galaxyhaze/Zith">
              View on GitHub
            </Link>
          </div>
        </section>

      </main>
    </Layout>
  );
}