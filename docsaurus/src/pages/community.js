import clsx from 'clsx';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import Giscus from '@giscus/react';
import styles from './community.module.css';

export default function Community() {
  const {siteConfig} = useDocusaurusContext();
  
  return (
    <Layout 
      title="Community" 
      description="Join the Zith community - Discord, GitHub, and more"
    >
      <main className={styles.main}>
        {/* Hero Section */}
        <section className={styles.hero}>
          <Heading as="h1">Join the Zith Community</Heading>
          <p className={styles.heroDescription}>
            Connect with developers, get help, share your projects, and shape the future of Zith.
          </p>
        </section>

        {/* Get Involved Cards */}
        <section className={styles.getInvolved}>
          <Heading as="h2">Get Involved</Heading>
          
          <div className={styles.cardsGrid}>
            <div className={styles.card}>
              <div className={styles.cardIcon}>
                <svg width="32" height="32" viewBox="0 0 24 24" fill="currentColor">
                  <path d="M20.317 4.37a19.791 19.791 0 0 0-4.885-1.515.074.074 0 0 0-.079.037c-.21.375-.444.864-.608 1.25a18.27 18.27 0 0 0-5.487 0 12.64 12.64 0 0 0-.617-1.25.077.077 0 0 0-.079-.037A19.736 19.736 0 0 0 3.677 4.37a.07.07 0 0 0-.032.027C.533 9.046-.32 13.58.099 18.057a.082.082 0 0 0 .031.057 19.9 19.9 0 0 0 5.993 3.03.078.078 0 0 0 .084-.028 14.09 14.09 0 0 0 1.226-1.994.076.076 0 0 0-.041-.106 13.107 13.107 0 0 1-1.872-.892.077.077 0 0 1-.008-.128 10.2 10.2 0 0 0 .372-.292.074.074 0 0 1 .077-.01c3.928 1.793 8.18 1.793 12.062 0a.074.074 0 0 1 .078.01c.12.098.246.198.373.292a.077.077 0 0 1-.006.127 12.299 12.299 0 0 1-1.873.892.077.077 0 0 0-.041.107c.36.698.772 1.362 1.225 1.993a.076.076 0 0 0 .084.028 19.839 19.839 0 0 0 6.002-3.03.077.077 0 0 0 .032-.054c.5-5.177-.838-9.674-3.549-13.66a.061.061 0 0 0-.031-.03zM8.02 15.33c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.956-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.333-.956 2.418-2.157 2.418zm7.975 0c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.955-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.333-.946 2.418-2.157 2.418z"/>
                </svg>
              </div>
              <h3>Discord</h3>
              <p>Real-time chat, help, and community discussions</p>
              <Link href="https://discord.gg/zith-lang" className="button button--primary">
                Join Discord
              </Link>
            </div>

            <div className={styles.card}>
              <div className={styles.cardIcon}>
                <svg width="32" height="32" viewBox="0 0 24 24" fill="currentColor">
                  <path d="M12 0C5.374 0 0 5.373 0 12c0 5.302 3.438 9.8 8.207 11.387.599.111.793-.261.793-.577v-2.234c-3.338.726-4.033-1.416-4.033-1.416-.546-1.387-1.333-1.756-1.333-1.756-1.089-.745.083-.729.083-.729 1.205.084 1.839 1.237 1.839 1.237 1.07 1.834 2.807 1.304 3.492.997.107-.775.418-1.305.762-1.604-2.665-.305-5.467-1.334-5.467-5.931 0-1.311.469-2.381 1.236-3.221-.124-.303-.535-1.524.117-3.176 0 0 1.008-.322 3.301 1.23A11.509 11.509 0 0112 5.803c1.02.005 2.047.138 3.006.404 2.291-1.552 3.297-1.23 3.297-1.23.653 1.653.242 2.874.118 3.176.77.84 1.235 1.911 1.235 3.221 0 4.609-2.807 5.624-5.479 5.921.43.372.823 1.102.823 2.222v3.293c0 .319.192.694.801.576C20.566 21.797 24 17.3 24 12c0-6.627-5.373-12-12-12z"/>
                </svg>
              </div>
              <h3>GitHub</h3>
              <p>Report bugs, request features, and contribute code</p>
              <Link href="https://github.com/GalaxyHaze/Zith" className="button button--secondary">
                View Repository
              </Link>
            </div>

            <div className={styles.card}>
              <div className={styles.cardIcon}>
                <svg width="32" height="32" viewBox="0 0 24 24" fill="currentColor">
                  <path d="M20.317 4.37a19.791 19.791 0 0 0-4.885-1.515.074.074 0 0 0-.079.037c-.21.375-.444.864-.608 1.25a18.27 18.27 0 0 0-5.487 0 12.64 12.64 0 0 0-.617-1.25.077.077 0 0 0-.079-.037A19.736 19.736 0 0 0 3.677 4.37a.07.07 0 0 0-.032.027C.533 9.046-.32 13.58.099 18.057a.082.082 0 0 0 .031.057 19.9 19.9 0 0 0 5.993 3.03.078.078 0 0 0 .084-.028 14.09 14.09 0 0 0 1.226-1.994.076.076 0 0 0-.041-.106 13.107 13.107 0 0 1-1.872-.892.077.077 0 0 1-.008-.128 10.2 10.2 0 0 0 .372-.292.074.074 0 0 1 .077-.01c3.928 1.793 8.18 1.793 12.062 0a.074.074 0 0 1 .078.01c.12.098.246.198.373.292a.077.077 0 0 1-.006.127 12.299 12.299 0 0 1-1.873.892.077.077 0 0 0-.041.107c.36.698.772 1.362 1.225 1.993a.076.076 0 0 0 .084.028 19.839 19.839 0 0 0 6.002-3.03.077.077 0 0 0 .032-.054c.5-5.177-.838-9.674-3.549-13.66a.061.061 0 0 0-.031-.03zM8.02 15.33c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.956-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.333-.956 2.418-2.157 2.418zm7.975 0c-1.183 0-2.157-1.085-2.157-2.419 0-1.333.955-2.419 2.157-2.419 1.21 0 2.176 1.096 2.157 2.42 0 1.333-.946 2.418-2.157 2.418z"/>
                </svg>
              </div>
              <h3>Discussions</h3>
              <p>Ask questions, share ideas, and connect with the community</p>
              <Link href="https://github.com/GalaxyHaze/Zith/discussions" className="button button--secondary">
                Join Discussions
              </Link>
            </div>
          </div>
        </section>

        {/* Discord Widget */}
        <section className={styles.discordSection}>
          <Heading as="h2">Chat With Us</Heading>
          <p className={styles.sectionSubtitle}>
            Join our Discord server to connect with other Zith developers in real-time.
          </p>
          
          <div className={styles.discordWidget}>
            <iframe
              src="https://discord.com/widget?id=1295261366230855781&theme=dark"
              width="100%"
              height="500"
              allowtransparency
              frameBorder="0"
              sandbox="allow-popups allow-popups-to-escape-sandbox allow-same-origin allow-scripts"
              title="Zith Discord Widget"
            />
          </div>
          
          <Link href="https://discord.gg/zith-lang" className={styles.discordButton}>
            Open in Discord
          </Link>
        </section>

        {/* Discussion Section */}
        <section className={styles.discussionsSection}>
          <Heading as="h2">Community Discussions</Heading>
          <p className={styles.sectionSubtitle}>
            Join the conversation on GitHub Discussions. Ask questions, share ideas, and help shape Zith's future.
          </p>
          
          <div className={styles.discussionsCTA}>
            <Link href="https://github.com/GalaxyHaze/Zith/discussions" className="button button--primary button--lg">
              Browse Discussions
            </Link>
          </div>
        </section>

        {/* Giscus Comments */}
        <section className={styles.commentsSection}>
          <Heading as="h2">Community Feedback</Heading>
          <p className={styles.sectionSubtitle}>
            Have ideas or suggestions? Leave a comment below to start a discussion.
          </p>
          
          <div className={styles.giscusWrapper}>
            <Giscus
              repo="GalaxyHaze/Zith"
              repoId="R_kgDOGs6uYw"
              category="General"
              categoryId="DIC_kwDOGs6uYw4CR-F_"
              mapping="title"
              reactionsEnabled="1"
              emitMetadata="0"
              inputPosition="top"
              theme="dark"
              lang="en"
              loading="lazy"
            />
          </div>
        </section>

        {/* Contributing CTA */}
        <section className={styles.contributingSection}>
          <Heading as="h2">Contribute to Zith</Heading>
          <p className={styles.sectionSubtitle}>
            Zith is an open-source project. We welcome contributions from developers of all skill levels.
          </p>
          
          <div className={styles.contributingLinks}>
            <Link to="/docs/community/contributing" className={styles.contributingLink}>
              <span className={styles.linkIcon}>→</span>
              <div>
                <h4>Contributing Guide</h4>
                <p>How to set up your dev environment and submit PRs</p>
              </div>
            </Link>
            
            <Link to="/docs/community/code-of-conduct" className={styles.contributingLink}>
              <span className={styles.linkIcon}>→</span>
              <div>
                <h4>Code of Conduct</h4>
                <p>Our community guidelines and expectations</p>
              </div>
            </Link>
          </div>
        </section>
      </main>
    </Layout>
  );
}